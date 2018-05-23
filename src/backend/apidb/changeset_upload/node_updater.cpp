
#include "util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/format.hpp>

using boost::format;

ApiDB_Node_Updater::ApiDB_Node_Updater(Transaction_Manager &_m,
                                       std::shared_ptr<OSMChange_Tracking> _ct)
    : m_bbox(), m(_m), ct(std::move(_ct)) {}

ApiDB_Node_Updater::~ApiDB_Node_Updater() {}

void ApiDB_Node_Updater::add_node(double lat, double lon,
                                  osm_changeset_id_t changeset_id,
                                  osm_nwr_signed_id_t old_id,
                                  const TagList &tags) {

  node_t new_node{};
  new_node.version = 1;
  new_node.lat = round(lat * SCALE);
  new_node.lon = round(lon * SCALE);
  new_node.tile = xy2tile(lon2x(lon), lat2y(lat));
  new_node.changeset_id = changeset_id;
  new_node.old_id = old_id;
  for (const auto &tag : tags)
    new_node.tags.emplace_back(
        std::pair<std::string, std::string>(tag.first, tag.second));
  create_nodes.push_back(new_node);

  ct->osmchange_orig_sequence.push_back({ operation::op_create,
                                          object_type::node, new_node.old_id,
                                          new_node.version, false });
}

void ApiDB_Node_Updater::modify_node(double lat, double lon,
                                     osm_changeset_id_t changeset_id,
                                     osm_nwr_id_t id, osm_version_t version,
                                     const TagList &tags) {

  node_t modify_node{};
  modify_node.id = id;
  modify_node.old_id = id;
  modify_node.version = version;
  modify_node.lat = round(lat * SCALE);
  modify_node.lon = round(lon * SCALE);
  modify_node.tile = xy2tile(lon2x(lon), lat2y(lat));
  modify_node.changeset_id = changeset_id;
  for (const auto &tag : tags)
    modify_node.tags.emplace_back(std::make_pair(tag.first, tag.second));
  modify_nodes.push_back(modify_node);

  ct->osmchange_orig_sequence.push_back({ operation::op_modify,
                                          object_type::node, modify_node.old_id,
                                          modify_node.version, false });
}

void ApiDB_Node_Updater::delete_node(osm_changeset_id_t changeset_id,
                                     osm_nwr_id_t id, osm_version_t version,
                                     bool if_unused) {

  node_t delete_node{};
  delete_node.id = id;
  delete_node.old_id = id;
  delete_node.version = version;
  delete_node.changeset_id = changeset_id;
  delete_node.if_unused = if_unused;
  delete_nodes.push_back(delete_node);

  ct->osmchange_orig_sequence.push_back({ operation::op_delete,
                                          object_type::node, delete_node.old_id,
                                          delete_node.version, if_unused });
}

void ApiDB_Node_Updater::process_new_nodes() {

  std::vector<osm_nwr_id_t> ids;

  truncate_temporary_tables();

  check_unique_placeholder_ids(create_nodes);

  insert_new_nodes_to_tmp_table(create_nodes);
  copy_tmp_create_nodes_to_current_nodes();
  delete_tmp_create_nodes();

  // Use new_ids as a result of inserting nodes in tmp table
  replace_old_ids_in_nodes(create_nodes, ct->created_node_ids);

  for (const auto &id : create_nodes)
    ids.push_back(id.id);

  // remove duplicates
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  lock_current_nodes(ids);

  insert_new_current_node_tags(create_nodes);
  save_current_nodes_to_history(ids);
  save_current_node_tags_to_history(ids);

  m_bbox.expand(calc_node_bbox(ids));

  create_nodes.clear();
}

void ApiDB_Node_Updater::process_modify_nodes() {

  std::vector<osm_nwr_id_t> ids;

  // Use new_ids as a result of inserting nodes in tmp table
  replace_old_ids_in_nodes(modify_nodes, ct->created_node_ids);

  for (const auto &id : modify_nodes)
    ids.push_back(id.id);

  // remove duplicates
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  lock_current_nodes(ids);

  // modify may contain several versions of the same node
  // those have to be processed one after the other
  auto packages = build_packages(modify_nodes);

  for (const auto &modify_nodes_package : packages) {
    std::vector<osm_nwr_id_t> ids_package;

    for (const auto &id : modify_nodes_package)
      ids_package.push_back(id.id);

    // remove duplicates
    std::sort(ids_package.begin(), ids_package.end());
    ids.erase(std::unique(ids_package.begin(), ids_package.end()),
              ids_package.end());

    check_current_node_versions(modify_nodes_package);

    m_bbox.expand(calc_node_bbox(ids_package));

    delete_current_node_tags(ids_package);
    update_current_nodes(modify_nodes_package);

    insert_new_current_node_tags(modify_nodes_package);
    save_current_nodes_to_history(ids_package);
    save_current_node_tags_to_history(ids_package);

    m_bbox.expand(calc_node_bbox(ids_package));
  }

  modify_nodes.clear();
}

void ApiDB_Node_Updater::process_delete_nodes() {

  std::vector<osm_nwr_id_t> ids;

  std::vector<node_t> delete_nodes_visible;
  std::vector<osm_nwr_id_t> ids_visible;
  std::vector<osm_nwr_id_t> ids_visible_unreferenced;

  // Use new_ids as a result of inserting nodes in tmp table
  replace_old_ids_in_nodes(delete_nodes, ct->created_node_ids);

  for (const auto &node : delete_nodes)
    ids.push_back(node.id);

  // remove duplicates
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  lock_current_nodes(ids);

  // In case the delete element has an "if-unused" flag, we ignore already
  // deleted nodes and avoid raising an error message.

  auto already_deleted_nodes = determine_already_deleted_nodes(delete_nodes);

  for (const auto &node : delete_nodes)
    if (already_deleted_nodes.find(node.id) == already_deleted_nodes.end()) {
      delete_nodes_visible.push_back(node);
      ids_visible.push_back(node.id);
    }

  check_current_node_versions(delete_nodes_visible);

  auto delete_nodes_visible_unreferenced =
      is_node_still_referenced(delete_nodes_visible);

  m_bbox.expand(calc_node_bbox(ids));

  delete_current_nodes(delete_nodes_visible_unreferenced);

  for (const auto &node : delete_nodes_visible_unreferenced)
    ids_visible_unreferenced.push_back(node.id);

  delete_current_node_tags(ids_visible_unreferenced);
  save_current_nodes_to_history(ids_visible_unreferenced);
  // no node tags to save here

  delete_nodes.clear();
}

void ApiDB_Node_Updater::truncate_temporary_tables() {
  m.exec("TRUNCATE TABLE tmp_create_nodes");
}

/*
 * Set id field based on old_id -> id mapping
 *
 */
void ApiDB_Node_Updater::replace_old_ids_in_nodes(
    std::vector<node_t> &nodes,
    const std::vector<OSMChange_Tracking::object_id_mapping_t>
        &created_node_id_mapping) {
  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map;

  for (auto i : created_node_id_mapping) {
    auto res = map.insert(
        std::pair<osm_nwr_signed_id_t, osm_nwr_id_t>(i.old_id, i.new_id));
    if (!res.second)
      throw http::bad_request(
          (boost::format("Duplicate node placeholder id %1%.") % i.old_id)
              .str());
  }

  for (auto &n : nodes) {
    if (n.old_id < 0) {
      auto entry = map.find(n.old_id);
      if (entry == map.end())
        throw http::bad_request(
            (boost::format("Placeholder id not found for node reference %1%") %
             n.old_id)
                .str());
      n.id = entry->second;
    }
  }
}

void ApiDB_Node_Updater::check_unique_placeholder_ids(
    const std::vector<node_t> &create_nodes) {

  for (const auto &create_node : create_nodes) {
    auto res = create_placedholder_ids.insert(create_node.old_id);

    if (!res.second)
      throw http::bad_request(
          "Placeholder IDs must be unique for created elements.");
  }
}

void ApiDB_Node_Updater::insert_new_nodes_to_tmp_table(
    const std::vector<node_t> &create_nodes) {

  m.prepare("insert_tmp_create_nodes",

            R"(
      WITH tmp_node(latitude, longitude, changeset_id, tile, old_id) AS (
         SELECT * FROM
         UNNEST( CAST($1 as integer[]),
                 CAST($2 as integer[]),
                 CAST($3 as bigint[]),
                 CAST($4 as bigint[]),
                 CAST($5 as bigint[])
               )
      )
      INSERT INTO tmp_create_nodes (latitude, longitude, changeset_id, tile, old_id)
      SELECT * FROM tmp_node
      RETURNING id, old_id
         )");

  std::vector<long> lats;
  std::vector<long> lons;
  std::vector<osm_changeset_id_t> cs;
  std::vector<long> tiles;
  std::vector<osm_nwr_signed_id_t> oldids;

  for (const auto &create_node : create_nodes) {
    lats.emplace_back(create_node.lat);
    lons.emplace_back(create_node.lon);
    cs.emplace_back(create_node.changeset_id);
    tiles.emplace_back(create_node.tile);
    oldids.emplace_back(create_node.old_id);
  }

  pqxx::result r =
      m.prepared("insert_tmp_create_nodes")(lats)(lons)(cs)(tiles)(oldids)
          .exec();

  if (r.affected_rows() != create_nodes.size())
    throw http::server_error(
        "Could not create all new nodes in temporary table");

  for (const auto &row : r)
    ct->created_node_ids.push_back({ row["old_id"].as<osm_nwr_signed_id_t>(),
                                     row["id"].as<osm_nwr_id_t>(), 1 });
}

void ApiDB_Node_Updater::copy_tmp_create_nodes_to_current_nodes() {

  auto r = m.exec(
      R"(
        INSERT INTO current_nodes ( 
             SELECT id, latitude, longitude, changeset_id, visible, 
                  timestamp, tile, version from tmp_create_nodes )
              )");
}

void ApiDB_Node_Updater::delete_tmp_create_nodes() {

  m.exec("DELETE FROM tmp_create_nodes");
}

void ApiDB_Node_Updater::lock_current_nodes(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("lock_current_nodes",
            "SELECT id FROM current_nodes WHERE id = ANY($1) FOR UPDATE");

  pqxx::result r = m.prepared("lock_current_nodes")(ids).exec();

  std::vector<osm_nwr_id_t> locked_ids;

  for (const auto &row : r)
    locked_ids.push_back(row["id"].as<osm_nwr_id_t>());

  if (ids.size() != locked_ids.size()) {
    std::set<osm_nwr_id_t> not_locked_ids;

    std::sort(locked_ids.begin(), locked_ids.end());

    std::set_difference(ids.begin(), ids.end(), locked_ids.begin(),
                        locked_ids.end(),
                        std::inserter(not_locked_ids, not_locked_ids.begin()));

    throw http::not_found(
        (boost::format(
             "The following node ids are not known on the database: %1%") %
         to_string(not_locked_ids))
            .str());
  }
}

/*
 * Multiple nodes with the same id cannot be processed in one step but have to
 * be spread across multiple packages
 * which are getting processed one after the other
 *
 */

std::vector<std::vector<ApiDB_Node_Updater::node_t>>
ApiDB_Node_Updater::build_packages(const std::vector<node_t> &nodes) {

  std::vector<std::vector<ApiDB_Node_Updater::node_t>> result;

  std::map<osm_nwr_id_t, unsigned int> id_to_package;

  for (const auto &node : nodes) {
    if (id_to_package.find(node.id) == id_to_package.end())
      id_to_package[node.id] = 0;
    else
      ++id_to_package[node.id];

    if (id_to_package[node.id] + 1 > result.size())
      result.emplace_back(std::vector<ApiDB_Node_Updater::node_t>());

    result.at(id_to_package[node.id]).emplace_back(node);
  }

  return result;
}

void ApiDB_Node_Updater::check_current_node_versions(
    const std::vector<node_t> &nodes) {
  // Assumption: All nodes exist on database, and are already locked by
  // lock_current_nodes

  if (nodes.empty())
    return;

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_version_t> versions;

  for (const auto &n : nodes) {
    ids.push_back(n.id);
    versions.push_back(n.version);
  }

  m.prepare("check_current_node_versions",
            R"(  

        WITH tmp_node_versions(id, version) AS (
            SELECT * FROM
            UNNEST( CAST($1 as bigint[]),
                    CAST($2 as bigint[])
           )
        )
        SELECT t.id, 
              t.version                  AS expected_version, 
              current_nodes.version      AS actual_version
        FROM tmp_node_versions t
        INNER JOIN current_nodes
           ON t.id = current_nodes.id
        WHERE t.version <> current_nodes.version
        LIMIT 1
          )");

  pqxx::result r =
      m.prepared("check_current_node_versions")(ids)(versions).exec();

  if (!r.empty()) {
    throw http::conflict(
        (boost::format(
             "Version mismatch: Provided %1%, server had: %2% of Node %3%") %
         r[0]["expected_version"].as<long>() %
         r[0]["actual_version"].as<long>() % r[0]["id"].as<long>())
            .str());
  }
}

// for if-unused - determine nodes to be excluded from deletion, regardless of
// their current version
std::set<osm_nwr_id_t> ApiDB_Node_Updater::determine_already_deleted_nodes(
    const std::vector<node_t> &nodes) {

  std::set<osm_nwr_id_t> result;

  if (nodes.empty())
    return result;

  std::set<osm_nwr_id_t> ids_to_be_deleted; // all node ids to be deleted
  std::set<osm_nwr_id_t> ids_if_unused; // node ids where if-used flag is set
  std::set<osm_nwr_id_t> ids_without_if_unused; // node ids without if-used flag

  for (const auto &node : nodes) {
    ids_to_be_deleted.insert(node.id);
    if (node.if_unused) {
      ids_if_unused.insert(node.id);
    } else {
      ids_without_if_unused.insert(node.id);
    }
  }

  if (ids_to_be_deleted.empty())
    return result;

  m.prepare("already_deleted_nodes", "SELECT id, version FROM current_nodes "
                                     "WHERE id = ANY($1) AND visible = false");

  pqxx::result r =
      m.prepared("already_deleted_nodes")(ids_to_be_deleted).exec();

  for (const auto &row : r) {

    osm_nwr_id_t id = row["id"].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a node that is already deleted,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.find(id) != ids_without_if_unused.end()) {

      throw http::gone(
          (boost::format("The node with the id %1% has already been deleted") %
           id).str());
    }

    result.insert(id);

    // if-used flag is set:
    // We have identified a node that is already deleted on the server. The only
    // thing left to do in this scenario is to return old_id, new_id and the
    // current version to the caller

    if (ids_if_unused.find(id) != ids_if_unused.end()) {

      ct->skip_deleted_node_ids.push_back(
          { row["id"].as<long>(), row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>() });
    }
  }

  return result;
}

bbox_t
ApiDB_Node_Updater::calc_node_bbox(const std::vector<osm_nwr_id_t> &ids) {

  bbox_t bbox;

  if (ids.empty())
    return bbox;

  m.prepare("calc_node_bbox",
            R"(
      SELECT MIN(latitude)  AS minlat,
             MIN(longitude) AS minlon, 
             MAX(latitude)  AS maxlat, 
             MAX(longitude) AS maxlon  
      FROM current_nodes WHERE id = ANY($1)
       )");

  pqxx::result r = m.prepared("calc_node_bbox")(ids).exec();

  if (!(r.empty() || r[0]["minlat"].is_null())) {
    bbox.minlat = r[0]["minlat"].as<long>();
    bbox.minlon = r[0]["minlon"].as<long>();
    bbox.maxlat = r[0]["maxlat"].as<long>();
    bbox.maxlon = r[0]["maxlon"].as<long>();
  }

  return bbox;
}

void ApiDB_Node_Updater::update_current_nodes(
    const std::vector<node_t> &nodes) {
  if (nodes.empty())
    return;

  m.prepare("update_current_nodes",
            R"(   
       WITH u(id, latitude, longitude, changeset_id, tile, version) AS (
          SELECT * FROM
          UNNEST( CAST($1 as bigint[]),
                  CAST($2 as integer[]),
                  CAST($3 as integer[]),
                  CAST($4 as bigint[]),
                  CAST($5 as bigint[]),
                  CAST($6 as bigint[])
                )
       )
       UPDATE current_nodes AS n
       SET latitude = u.latitude,
          longitude = u.longitude,
          changeset_id = u.changeset_id,
          visible = true,
          timestamp = (now() at time zone 'utc'),
          tile = u.tile,
          version = u.version + 1
          FROM u
        WHERE n.id = u.id
        AND   n.version = u.version
        RETURNING n.id, n.version
       )");

  std::vector<osm_nwr_id_t> ids;
  std::vector<long> lats;
  std::vector<long> lons;
  std::vector<osm_changeset_id_t> cs;
  std::vector<long> tiles;
  std::vector<osm_version_t> versions;

  for (const auto &node : nodes) {
    ids.emplace_back(node.id);
    lats.emplace_back(node.lat);
    lons.emplace_back(node.lon);
    cs.emplace_back(node.changeset_id);
    tiles.emplace_back(node.tile);
    versions.emplace_back(node.version);
  }

  pqxx::result r =
      m.prepared("update_current_nodes")(ids)(lats)(lons)(cs)(tiles)(versions)
          .exec();

  if (r.affected_rows() != nodes.size()) {
    std::set<osm_nwr_id_t> ids_set(ids.begin(), ids.end());
    std::set<osm_nwr_id_t> processed_ids;

    for (const auto &row : r)
      processed_ids.insert(row["id"].as<osm_nwr_id_t>());

    std::set<osm_nwr_id_t> diff;

    std::set_difference(ids_set.begin(), ids_set.end(), processed_ids.begin(),
                        processed_ids.end(), std::inserter(diff, diff.end()));

    auto unknown_id = (*diff.begin()); // Just take first element
    auto unknown_node =
        std::find_if(nodes.begin(), nodes.end(),
                     [&](const node_t &n) { return n.id == unknown_id; });

    throw http::server_error(
        (boost::format(
             "Could not update Node %1% with version %2% on the database.") %
         (*unknown_node).id % (*unknown_node).version)
            .str());
  }

  // update modified nodes table
  for (auto row : r)
    ct->modified_node_ids.push_back({ row["id"].as<long>(),
                                      row["id"].as<osm_nwr_id_t>(),
                                      row["version"].as<osm_version_t>() });
}

void ApiDB_Node_Updater::delete_current_nodes(
    const std::vector<node_t> &nodes) {

  if (nodes.empty())
    return;

  m.prepare("delete_current_nodes", R"(   

         WITH u(id, changeset_id, version) AS (
            SELECT * FROM
            UNNEST( CAST($1 as bigint[]),
                    CAST($2 as bigint[]),
                    CAST($3 as bigint[])
                )
         )
         UPDATE current_nodes AS n
         SET changeset_id = u.changeset_id,
            visible = false,
            timestamp = (now() at time zone 'utc'),
            version = u.version + 1
            FROM u
         WHERE n.id = u.id
         AND   n.version = u.version
         RETURNING n.id, n.version 
   )");

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_changeset_id_t> cs;
  std::vector<osm_version_t> versions;

  for (const auto &node : nodes) {
    ids.emplace_back(node.id);
    cs.emplace_back(node.changeset_id);
    versions.emplace_back(node.version);
  }

  pqxx::result r = m.prepared("delete_current_nodes")(ids)(cs)(versions).exec();

  if (r.affected_rows() != nodes.size())
    throw http::server_error("Could not delete all current nodes");

  // update deleted nodes table
  for (const auto &row : r)
    ct->deleted_node_ids.push_back({ row["id"].as<osm_nwr_id_t>() });
}

void ApiDB_Node_Updater::insert_new_current_node_tags(
    const std::vector<node_t> &nodes) {

  if (nodes.empty())
    return;

  m.prepare("insert_new_current_node_tags",

            R"(
      WITH tmp_tag(node_id, k, v) AS (
         SELECT * FROM
         UNNEST( CAST($1 AS bigint[]),
                 CAST($2 AS character varying[]),
                 CAST($3 AS character varying[])
         )
      )
      INSERT INTO current_node_tags(node_id, k, v)
      SELECT * FROM tmp_tag
         )");

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  unsigned total_tags = 0;

  for (const auto &node : nodes)
    for (const auto &tag : node.tags) {
      ids.emplace_back(node.id);
      ks.emplace_back(escape(tag.first));
      vs.emplace_back(escape(tag.second));
      ++total_tags;
    }

  if (total_tags == 0)
    return;

  pqxx::result r =
      m.prepared("insert_new_current_node_tags")(ids)(ks)(vs).exec();

  if (r.affected_rows() != total_tags)
    throw http::server_error("Could not create new current node tags");
}

void ApiDB_Node_Updater::save_current_nodes_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  // current_nodes -> nodes

  if (ids.empty())
    return;

  m.prepare("current_nodes_to_history",
            R"(   
         INSERT INTO nodes 
             (SELECT id AS node_id, latitude, longitude,
                   changeset_id, visible, timestamp, tile, version
              FROM current_nodes
              WHERE id = ANY($1) 
             )
       )");

  pqxx::result r = m.prepared("current_nodes_to_history")(ids).exec();

  if (r.affected_rows() != ids.size())
    throw http::server_error("Could not save current nodes to history");
}

void ApiDB_Node_Updater::save_current_node_tags_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  // current_node_tags -> node_tags

  if (ids.empty())
    return;

  m.prepare("current_node_tags_to_history",
            R"(   
            INSERT INTO node_tags ( 
                SELECT node_id, version, k, v 
                FROM current_node_tags t
                  INNER JOIN current_nodes n
                     ON t.node_id = n.id 
                WHERE id = ANY($1) )
           )");

  pqxx::result r = m.prepared("current_node_tags_to_history")(ids).exec();
}

std::vector<ApiDB_Node_Updater::node_t>
ApiDB_Node_Updater::is_node_still_referenced(const std::vector<node_t> &nodes) {
  // check if node id is still referenced in ways or relations

  if (nodes.empty())
    return nodes;

  std::vector<osm_nwr_id_t> ids;
  std::set<osm_nwr_id_t> ids_if_unused; // node ids where if-used flag is set
  std::set<osm_nwr_id_t> ids_without_if_unused; // node ids without if-used flag

  for (const auto &id : nodes) {
    ids.push_back(id.id);
    if (id.if_unused) {
      ids_if_unused.insert(id.id);
    } else {
      ids_without_if_unused.insert(id.id);
    }
  }

  std::vector<node_t> updated_nodes = nodes;
  std::set<osm_nwr_id_t> nodes_to_exclude_from_deletion;

  {
    m.prepare("node_still_referenced_by_way",
              R"(   
            SELECT current_way_nodes.node_id, array_agg(current_ways.id) AS way_ids
               FROM current_ways 
                 INNER JOIN current_way_nodes
                   ON current_way_nodes.way_id = current_ways.id
               WHERE current_ways.visible = true
                 AND current_way_nodes.node_id = ANY($1)
               GROUP BY current_way_nodes.node_id
            )");

    pqxx::result r = m.prepared("node_still_referenced_by_way")(ids).exec();

    for (const auto &row : r) {
      auto node_id = row["node_id"].as<osm_nwr_id_t>();

      // OsmChange documents wants to delete a node that is still referenced,
      // and the if-unused flag hasn't been set!
      if (ids_without_if_unused.find(node_id) != ids_without_if_unused.end()) {

        // Without the if-unused, such a situation would lead to an error, and
        // the whole diff upload would fail.
        throw http::precondition_failed(
            (boost::format("Node %1% is still used by ways %2%.") %
             row["node_id"].as<osm_nwr_id_t>() %
             friendly_name(row["way_ids"].as<std::string>()))
                .str());
      }

      if (ids_if_unused.find(node_id) != ids_if_unused.end()) {
        /* a <delete> block in the OsmChange document may have an if-unused
         * attribute
         * If this attribute is present, then the delete operation(s) in this
         * block
         * are conditional and will only be executed if the object to be deleted
         * is not used by another object.  */

        nodes_to_exclude_from_deletion.insert(
            row["node_id"].as<osm_nwr_id_t>());
      }
    }
  }

  {
    m.prepare("node_still_referenced_by_relation",
              R"(   
             SELECT current_relation_members.member_id, array_agg(current_relations.id) AS relation_ids 
                    FROM current_relations 
                      INNER JOIN current_relation_members
                        ON current_relation_members.relation_id = current_relations.id
                    WHERE current_relations.visible = true
                      AND current_relation_members.member_type = 'Node'
                      AND current_relation_members.member_id = ANY($1)
                    GROUP BY current_relation_members.member_id
             )");

    pqxx::result r =
        m.prepared("node_still_referenced_by_relation")(ids).exec();

    for (const auto &row : r) {
      auto node_id = row["member_id"].as<osm_nwr_id_t>();

      // OsmChange documents wants to delete a node that is still referenced,
      // and the if-unused flag hasn't been set!
      if (ids_without_if_unused.find(node_id) != ids_without_if_unused.end()) {

        // Without the if-unused, such a situation would lead to an error, and
        // the whole diff upload would fail.
        throw http::precondition_failed(
            (boost::format("Node %1% is still used by relations %2%.") %
             row["member_id"].as<osm_nwr_id_t>() %
             friendly_name(row["relation_ids"].as<std::string>()))
                .str());
      }

      if (ids_if_unused.find(node_id) != ids_if_unused.end())
        nodes_to_exclude_from_deletion.insert(
            row["member_id"].as<osm_nwr_id_t>());
    }
  }

  // Prepare updated list of nodes, which no longer contains object that are
  // still in use by ways/relations
  // We will simply skip those nodes from now on

  if (!nodes_to_exclude_from_deletion.empty()) {
    updated_nodes.erase(
        std::remove_if(updated_nodes.begin(), updated_nodes.end(),
                       [&](const node_t &a) {
                         return nodes_to_exclude_from_deletion.find(a.id) !=
                                nodes_to_exclude_from_deletion.end();
                       }),
        updated_nodes.end());

    // Return old_id, new_id and current version to the caller in case of
    // if-unused, so it's clear that the delete operation was *not* executed,
    // but simply skipped

    m.prepare("still_referenced_nodes",
              "SELECT id, version FROM current_nodes WHERE id = ANY($1)");

    pqxx::result r =
        m.prepared("still_referenced_nodes")(nodes_to_exclude_from_deletion)
            .exec();

    if (r.affected_rows() != nodes_to_exclude_from_deletion.size())
      throw http::server_error(
          "Could not get details about still referenced nodes");

    std::set<osm_nwr_id_t> result;

    for (const auto &row : r) {
      result.insert(row["id"].as<osm_nwr_id_t>());

      // We have identified a node that is still used in a way or relation.
      // However, the caller has indicated via if-unused flag that deletion
      // should not lead to an error. All we can do now is to return old_id,
      // new_id and the current version to the caller

      ct->skip_deleted_node_ids.push_back(
          { row["id"].as<long>(), row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>() });
    }
  }

  return updated_nodes;
}

void ApiDB_Node_Updater::delete_current_node_tags(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("delete_current_node_tags",
            "DELETE FROM current_node_tags WHERE node_id = ANY($1)");

  pqxx::result r = m.prepared("delete_current_node_tags")(ids).exec();
}

unsigned int ApiDB_Node_Updater::get_num_changes() {
  return (ct->created_node_ids.size() + ct->modified_node_ids.size() +
          ct->deleted_node_ids.size());
}

bbox_t ApiDB_Node_Updater::bbox() { return m_bbox; }
