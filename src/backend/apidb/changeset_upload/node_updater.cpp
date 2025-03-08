/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/http.hpp"
#include "cgimap/options.hpp"
#include "cgimap/util.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <fmt/core.h>



ApiDB_Node_Updater::ApiDB_Node_Updater(Transaction_Manager &_m,
                                       const RequestContext& _req_ctx,
                                       api06::OSMChange_Tracking &ct)
    : m(_m),
      req_ctx(_req_ctx),
      ct(ct)
{}

void ApiDB_Node_Updater::add_node(double lat, double lon,
                                  osm_changeset_id_t changeset_id,
                                  osm_nwr_signed_id_t old_id,
                                  const api06::TagList &tags) {

  node_t new_node{ .version = 1,
                   .lat = static_cast<int64_t>(round(lat * global_settings::get_scale())),
                   .lon = static_cast<int64_t>(round(lon * global_settings::get_scale())),
                   .tile = xy2tile(lon2x(lon), lat2y(lat)),
                   .changeset_id = changeset_id,
                   .old_id = old_id};

  for (const auto &[key, value] : tags)
    new_node.tags.emplace_back(key, value);
  create_nodes.push_back(new_node);

  ct.osmchange_orig_sequence.push_back({ operation::op_create,
                                          object_type::node, new_node.old_id,
                                          new_node.version, false });
}

void ApiDB_Node_Updater::modify_node(double lat, double lon,
                                     osm_changeset_id_t changeset_id,
                                     osm_nwr_id_t id, osm_version_t version,
                                     const api06::TagList &tags) {

  node_t modify_node{ .id = id,
                      .version = version,
                      .lat = static_cast<int64_t>(round(lat * global_settings::get_scale())),
                      .lon = static_cast<int64_t>(round(lon * global_settings::get_scale())),
                      .tile = xy2tile(lon2x(lon), lat2y(lat)),
                      .changeset_id = changeset_id,
                      .old_id = static_cast<osm_nwr_signed_id_t>(id)};

  for (const auto &[key, value] : tags)
    modify_node.tags.emplace_back(key, value);
  modify_nodes.push_back(modify_node);

  ct.osmchange_orig_sequence.push_back({ operation::op_modify,
                                          object_type::node, modify_node.old_id,
                                          modify_node.version, false });
}

void ApiDB_Node_Updater::delete_node(osm_changeset_id_t changeset_id,
                                     osm_nwr_id_t id, osm_version_t version,
                                     bool if_unused) {

  node_t delete_node{ .id = id,
                      .version = version,
                      .changeset_id = changeset_id,
                      .old_id = static_cast<osm_nwr_signed_id_t>(id),
                      .if_unused = if_unused};

  delete_nodes.push_back(delete_node);

  ct.osmchange_orig_sequence.push_back({ operation::op_delete,
                                          object_type::node, delete_node.old_id,
                                          delete_node.version, if_unused });
}

void ApiDB_Node_Updater::process_new_nodes() {

  std::vector<osm_nwr_id_t> ids;

  check_unique_placeholder_ids(create_nodes);

  insert_new_nodes_to_current_table(create_nodes);

  // Use new_ids as a result of inserting nodes in tmp table
  replace_old_ids_in_nodes(create_nodes, ct.created_node_ids);

  ids.reserve(create_nodes.size());

  for (const auto &id : create_nodes)
    ids.emplace_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  // lock_current_nodes(ids);    // INSERT already set RowExclusiveLock earlier on

  const auto ids_with_tags = insert_new_current_node_tags(create_nodes);
  save_current_nodes_to_history(ids);
  save_current_node_tags_to_history(ids_with_tags);

  m_bbox.expand(calc_node_bbox(ids));

  create_nodes.clear();
}

void ApiDB_Node_Updater::process_modify_nodes() {

  std::vector<osm_nwr_id_t> ids;

  // Use new_ids as a result of inserting nodes in tmp table
  replace_old_ids_in_nodes(modify_nodes, ct.created_node_ids);

  for (const auto &id : modify_nodes)
    ids.push_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  lock_current_nodes(ids);

  // modify may contain several versions of the same node
  // those have to be processed one after the other
  auto packages = build_packages(modify_nodes);

  for (const auto &modify_nodes_package : packages) {
    std::vector<osm_nwr_id_t> ids_package;
    ids_package.reserve(modify_nodes_package.size());

    for (const auto &id : modify_nodes_package)
      ids_package.push_back(id.id);

    // remove duplicates
    std::ranges::sort(ids_package);
    auto new_end = std::ranges::unique(ids_package);
    ids_package.erase(new_end.begin(), new_end.end());

    check_current_node_versions(modify_nodes_package);

    m_bbox.expand(calc_node_bbox(ids_package));

    delete_current_node_tags(ids_package);
    update_current_nodes(modify_nodes_package);

    const auto ids_with_tags = insert_new_current_node_tags(modify_nodes_package);
    save_current_nodes_to_history(ids_package);
    save_current_node_tags_to_history(ids_with_tags);

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
  replace_old_ids_in_nodes(delete_nodes, ct.created_node_ids);

  for (const auto &node : delete_nodes)
    ids.push_back(node.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  lock_current_nodes(ids);

  // In case the delete element has an "if-unused" flag, we ignore already
  // deleted nodes and avoid raising an error message.

  auto already_deleted_nodes = determine_already_deleted_nodes(delete_nodes);

  for (const auto &node : delete_nodes)
    if (!already_deleted_nodes.contains(node.id)) {
      delete_nodes_visible.push_back(node);
      ids_visible.push_back(node.id);
    }

  check_current_node_versions(delete_nodes_visible);

  auto delete_nodes_visible_unreferenced =
      is_node_still_referenced(delete_nodes_visible);

  m_bbox.expand(calc_node_bbox(ids));

  delete_current_nodes(delete_nodes_visible_unreferenced);

  ids_visible_unreferenced.reserve(delete_nodes_visible_unreferenced.size());

  for (const auto &node : delete_nodes_visible_unreferenced)
    ids_visible_unreferenced.push_back(node.id);

  delete_current_node_tags(ids_visible_unreferenced);
  save_current_nodes_to_history(ids_visible_unreferenced);
  // no node tags to save here

  delete_nodes.clear();
}

/*
 * Set id field based on old_id -> id mapping
 *
 */
void ApiDB_Node_Updater::replace_old_ids_in_nodes(
    std::vector<node_t> &nodes,
    const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
        &created_node_id_mapping) {
  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map;

  for (auto &i : created_node_id_mapping) {
    auto [_, inserted] = map.insert( { i.old_id, i.new_id } );
    if (!inserted)
      throw http::bad_request(
          fmt::format("Duplicate node placeholder id {:d}.", i.old_id));
  }

  for (auto &n : nodes) {
    if (n.old_id < 0) {
      auto entry = map.find(n.old_id);
      if (entry == map.end())
        throw http::bad_request(
            fmt::format("Placeholder id not found for node reference {:d}", n.old_id));
      n.id = entry->second;
    }
  }
}

void ApiDB_Node_Updater::check_unique_placeholder_ids(
    const std::vector<node_t> &create_nodes) {

  for (const auto &create_node : create_nodes) {
    auto [_, inserted] = create_placeholder_ids.insert(create_node.old_id);

    if (!inserted)
      throw http::bad_request(
          "Placeholder IDs must be unique for created elements.");
  }
}

void ApiDB_Node_Updater::insert_new_nodes_to_current_table(
    const std::vector<node_t> &create_nodes) {

  if (create_nodes.empty())
    return;

  // Note: fetching next sequence values has been moved from CTE tmp_nodes
  // to a dedicated CTE ids_mapping in order to avoid small gaps in the sequence.
  // Postgresql appears to have called nextval() once too often otherwise.

  m.prepare("insert_new_nodes_to_current_table", R"(

       WITH ids_mapping AS (
        SELECT nextval('current_nodes_id_seq'::regclass) AS id,
               old_id
        FROM
           UNNEST($5::bigint[]) AS id(old_id)
       ),
       tmp_nodes AS (
         SELECT
           UNNEST($1::integer[]) AS latitude,
           UNNEST($2::integer[]) AS longitude,
           UNNEST($3::bigint[]) AS changeset_id,
           true::boolean AS visible,
           (now() at time zone 'utc')::timestamp without time zone AS "timestamp",
           UNNEST($4::bigint[]) AS tile,
           1::bigint AS version,
           UNNEST($5::bigint[]) AS old_id
      ),
      insert_op AS (
        INSERT INTO current_nodes (id, latitude, longitude, changeset_id,
                  visible, timestamp, tile, version)
             SELECT id, latitude, longitude, changeset_id, visible,
                  timestamp, tile, version FROM tmp_nodes
             INNER JOIN ids_mapping
             ON tmp_nodes.old_id = ids_mapping.old_id
      )
      SELECT id, old_id
        FROM ids_mapping
  )"_M);

  std::vector<int64_t> lats;
  std::vector<int64_t> lons;
  std::vector<osm_changeset_id_t> cs;
  std::vector<uint64_t> tiles;
  std::vector<osm_nwr_signed_id_t> oldids;

  lats.reserve(create_nodes.size());
  lons.reserve(create_nodes.size());
  cs.reserve(create_nodes.size());
  tiles.reserve(create_nodes.size());
  oldids.reserve(create_nodes.size());

  for (const auto &create_node : create_nodes) {
    lats.emplace_back(create_node.lat);
    lons.emplace_back(create_node.lon);
    cs.emplace_back(create_node.changeset_id);
    tiles.emplace_back(create_node.tile);
    oldids.emplace_back(create_node.old_id);
  }

  auto r = m.exec_prepared("insert_new_nodes_to_current_table", lats, lons, cs, tiles, oldids);

  if (r.affected_rows() != create_nodes.size())
    throw http::server_error(
        "Could not create all new nodes in current_nodes table");

  const auto old_id_col(r.column_number("old_id"));
  const auto id_col(r.column_number("id"));

  for (const auto &row : r) {
    ct.created_node_ids.push_back({row[old_id_col].as<osm_nwr_signed_id_t>(),
                                  row[id_col].as<osm_nwr_id_t>(), 1});
  }

}

void ApiDB_Node_Updater::lock_current_nodes(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("lock_current_nodes", R"(
      WITH locked AS (
        SELECT id FROM current_nodes WHERE id = ANY($1) FOR UPDATE
      )
      SELECT t.id FROM UNNEST($1) AS t(id)
      EXCEPT
      SELECT id FROM locked
      ORDER BY id
     )"_M);

  // Query returns only node ids, which could not be locked
  auto r = m.exec_prepared("lock_current_nodes", ids);

  if (!r.empty()) {
    std::vector<osm_nwr_id_t> missing_ids;
    missing_ids.reserve(ids.size());

    const auto id_col(r.column_number("id"));

    for (const auto &row : r)
      missing_ids.push_back(row[id_col].as<osm_nwr_id_t>());

    throw http::not_found(
        fmt::format("The following node ids are not known on the database: {}", to_string(missing_ids)));
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
    if (!id_to_package.contains(node.id))
      id_to_package[node.id] = 0;
    else
      ++id_to_package[node.id];

    if (id_to_package[node.id] + 1 > result.size())
      result.emplace_back();

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

  ids.reserve(nodes.size());
  versions.reserve(nodes.size());

  for (const auto &n : nodes) {
    ids.emplace_back(n.id);
    versions.emplace_back(n.version);
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
              t.version AS expected_version,
              cn.version AS actual_version
        FROM tmp_node_versions t
        INNER JOIN current_nodes cn
           ON t.id = cn.id
        WHERE t.version <> cn.version
        LIMIT 1
          )"_M);

  auto r = m.exec_prepared("check_current_node_versions", ids, versions);

  if (!r.empty()) {
    throw http::conflict(
        fmt::format(
             "Version mismatch: Provided {:d}, server had: {:d} of Node {:d}",
         r[0]["expected_version"].as<osm_version_t>(),
         r[0]["actual_version"].as<osm_version_t>(),
         r[0]["id"].as<osm_nwr_id_t>()));
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
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  for (const auto &node : nodes) {
    ids_to_be_deleted.insert(node.id);
    if (node.if_unused) {
      ids_if_unused.insert(node.id);
    } else {
      ids_without_if_unused.insert(node.id);
    }
    id_to_old_id[node.id] = node.old_id;
  }

  if (ids_to_be_deleted.empty())
    return result;

  m.prepare("already_deleted_nodes", "SELECT id, version FROM current_nodes "
                                     "WHERE id = ANY($1) AND visible = false");

  auto r = m.exec_prepared("already_deleted_nodes", ids_to_be_deleted);

  const auto id_col(r.column_number("id"));

  for (const auto &row : r) {

    auto id = row[id_col].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a node that is already deleted,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.contains(id)) {

      throw http::gone(
          fmt::format("The node with the id {:d} has already been deleted", id));
    }

    result.insert(id);

    // if-used flag is set:
    // We have identified a node that is already deleted on the server. The only
    // thing left to do in this scenario is to return old_id, new_id and the
    // current version to the caller

    if (ids_if_unused.contains(id)) {

      ct.skip_deleted_node_ids.push_back({
            id_to_old_id[ row["id"].as<osm_nwr_id_t>() ],
	          row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>()});
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
       )"_M);

  auto r = m.exec_prepared("calc_node_bbox", ids);

  if (!(r.empty() || r[0]["minlat"].is_null())) {
    bbox.minlat = r[0]["minlat"].as<int64_t>();
    bbox.minlon = r[0]["minlon"].as<int64_t>();
    bbox.maxlat = r[0]["maxlat"].as<int64_t>();
    bbox.maxlon = r[0]["maxlon"].as<int64_t>();
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
       )"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<int64_t> lats;
  std::vector<int64_t> lons;
  std::vector<osm_changeset_id_t> cs;
  std::vector<uint64_t> tiles;
  std::vector<osm_version_t> versions;
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  ids.reserve(nodes.size());
  lats.reserve(nodes.size());
  lons.reserve(nodes.size());
  cs.reserve(nodes.size());
  tiles.reserve(nodes.size());
  versions.reserve(nodes.size());

  for (const auto &node : nodes) {
    ids.emplace_back(node.id);
    lats.emplace_back(node.lat);
    lons.emplace_back(node.lon);
    cs.emplace_back(node.changeset_id);
    tiles.emplace_back(node.tile);
    versions.emplace_back(node.version);
    id_to_old_id[node.id] = node.old_id;
  }

  auto r = m.exec_prepared("update_current_nodes", ids, lats, lons, cs, tiles, versions);

  if (r.affected_rows() != nodes.size()) {
    std::set<osm_nwr_id_t> ids_set(ids.begin(), ids.end());
    std::set<osm_nwr_id_t> processed_ids;

    for (const auto &row : r)
      processed_ids.insert(row["id"].as<osm_nwr_id_t>());

    std::set<osm_nwr_id_t> diff;

    std::ranges::set_difference(ids_set, processed_ids, std::inserter(diff, diff.end()));

    auto unknown_id = (*diff.begin()); // Just take first element
    auto unknown_node =
        std::ranges::find_if(nodes, [&](const node_t &n) { return n.id == unknown_id; });

    throw http::server_error(
        fmt::format(
             "Could not update Node {:d} with version {:d} on the database.",
         (*unknown_node).id,
         (*unknown_node).version));
  }

  const auto id_col(r.column_number("id"));
  const auto version_col(r.column_number("version"));

  // update modified nodes table
  for (const auto &row : r)
    ct.modified_node_ids.push_back({id_to_old_id[row[id_col].as<osm_nwr_id_t>()],
                                   row[id_col].as<osm_nwr_id_t>(),
                                   row[version_col].as<osm_version_t>()});
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
   )"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_changeset_id_t> cs;
  std::vector<osm_version_t> versions;
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  ids.reserve(nodes.size());
  cs.reserve(nodes.size());
  versions.reserve(nodes.size());

  for (const auto &node : nodes) {
    ids.emplace_back(node.id);
    cs.emplace_back(node.changeset_id);
    versions.emplace_back(node.version);
    id_to_old_id[node.id] = node.old_id;
  }

  auto r = m.exec_prepared("delete_current_nodes", ids, cs, versions);

  if (r.affected_rows() != nodes.size())
    throw http::server_error("Could not delete all current nodes");


  const auto id_col(r.column_number("id"));

  // update deleted nodes table
  for (const auto &row : r)
    ct.deleted_node_ids.push_back({ id_to_old_id[row[id_col].as<osm_nwr_id_t>()] });
}

std::vector<osm_nwr_id_t> ApiDB_Node_Updater::insert_new_current_node_tags(
    const std::vector<node_t> &nodes) {

  if (nodes.empty())
    return {};

#if PQXX_VERSION_MAJOR < 7

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
         )"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  ids.reserve(nodes.size());

  unsigned total_tags = 0;

  for (const auto &node : nodes) {
    for (const auto &[key, value] : node.tags) {
      ids.emplace_back(node.id);
      ks.emplace_back(escape(key));
      vs.emplace_back(escape(value));
      ++total_tags;
    }
  }

  if (total_tags == 0)
    return {};

  auto r = m.exec_prepared("insert_new_current_node_tags", ids, ks, vs);

  if (r.affected_rows() != total_tags)
    throw http::server_error("Could not create new current node tags");

#else

  std::vector<osm_nwr_id_t> ids;

  auto stream = m.to_stream("current_node_tags", "node_id, k, v");

  for (const auto &node : nodes) {
    for (const auto &tag : node.tags) {
      stream.write_values(node.id, tag.first, tag.second);
      ids.emplace_back(node.id);
    }
  }

  stream.complete();

#endif

  // prepare list of node ids with tags
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());
  return ids;
}

void ApiDB_Node_Updater::save_current_nodes_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  // current_nodes -> nodes

  if (ids.empty())
    return;

  m.prepare("current_nodes_to_history",
            R"(
         INSERT INTO nodes (node_id, latitude, longitude, changeset_id,
                   visible, timestamp, tile, version)
              SELECT id, latitude, longitude,
                   changeset_id, visible, timestamp, tile, version
              FROM current_nodes
              WHERE id = ANY($1)
       )"_M);

  auto r = m.exec_prepared("current_nodes_to_history", ids);

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
            INSERT INTO node_tags (node_id, version, k, v)
                SELECT node_id, version, k, v
                FROM current_node_tags t
                  INNER JOIN current_nodes n
                     ON t.node_id = n.id
                WHERE id = ANY($1)
           )"_M);

  auto r = m.exec_prepared("current_node_tags_to_history", ids);
}

std::vector<ApiDB_Node_Updater::node_t>
ApiDB_Node_Updater::is_node_still_referenced(const std::vector<node_t> &nodes) {
  // check if node id is still referenced in ways or relations

  if (nodes.empty())
    return nodes;

  std::vector<osm_nwr_id_t> ids;
  std::set<osm_nwr_id_t> ids_if_unused; // node ids where if-used flag is set
  std::set<osm_nwr_id_t> ids_without_if_unused; // node ids without if-used flag
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  for (const auto &node : nodes) {
    ids.push_back(node.id);
    if (node.if_unused) {
      ids_if_unused.insert(node.id);
    } else {
      ids_without_if_unused.insert(node.id);
    }
    id_to_old_id[node.id] = node.old_id;
  }

  std::vector<node_t> updated_nodes = nodes;
  std::set<osm_nwr_id_t> nodes_to_exclude_from_deletion;

  {
    m.prepare("node_still_referenced_by_way",
              R"(
            SELECT node_id,
                   string_agg(distinct way_id::text,',') AS way_ids
                   FROM current_way_nodes
                   WHERE node_id = ANY($1)
                   GROUP BY node_id
            )"_M);

    auto r = m.exec_prepared("node_still_referenced_by_way", ids);

    for (const auto &row : r) {
      auto node_id = row["node_id"].as<osm_nwr_id_t>();

      // OsmChange documents wants to delete a node that is still referenced,
      // and the if-unused flag hasn't been set!
      if (ids_without_if_unused.contains(node_id)) {

        // Without the if-unused, such a situation would lead to an error, and
        // the whole diff upload would fail.
        throw http::precondition_failed(
            fmt::format("Node {:d} is still used by ways {}.",
             row["node_id"].as<osm_nwr_id_t>(),
             row["way_ids"].c_str()));
      }

      if (ids_if_unused.contains(node_id)) {
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
             SELECT member_id,
                    string_agg(distinct relation_id::text,',') AS relation_ids
                    FROM current_relation_members
                    WHERE member_type = 'Node'
                      AND member_id = ANY($1)
                    GROUP BY member_id
             )"_M);

    auto r = m.exec_prepared("node_still_referenced_by_relation", ids);

    for (const auto &row : r) {
      auto node_id = row["member_id"].as<osm_nwr_id_t>();

      // OsmChange documents wants to delete a node that is still referenced,
      // and the if-unused flag hasn't been set!
      if (ids_without_if_unused.contains(node_id)) {

        // Without the if-unused, such a situation would lead to an error, and
        // the whole diff upload would fail.
        throw http::precondition_failed(
            fmt::format("Node {:d} is still used by relations {}.",
                row["member_id"].as<osm_nwr_id_t>(),
                row["relation_ids"].c_str()));
      }

      if (ids_if_unused.contains(node_id))
        nodes_to_exclude_from_deletion.insert(
            row["member_id"].as<osm_nwr_id_t>());
    }
  }

  // Prepare updated list of nodes, which no longer contains object that are
  // still in use by ways/relations
  // We will simply skip those nodes from now on

  if (!nodes_to_exclude_from_deletion.empty()) {
    std::erase_if(updated_nodes, [&](const node_t &a) {
                         return nodes_to_exclude_from_deletion.contains(a.id);
                       });

    // Return old_id, new_id and current version to the caller in case of
    // if-unused, so it's clear that the delete operation was *not* executed,
    // but simply skipped

    m.prepare("still_referenced_nodes",
              "SELECT id, version FROM current_nodes WHERE id = ANY($1)");

    auto r = m.exec_prepared("still_referenced_nodes", nodes_to_exclude_from_deletion);

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

      ct.skip_deleted_node_ids.push_back(
          { id_to_old_id[row["id"].as<osm_nwr_id_t>()],
	          row["id"].as<osm_nwr_id_t>(),
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

  auto r = m.exec_prepared("delete_current_node_tags", ids);
}

uint32_t ApiDB_Node_Updater::get_num_changes() const {
  return (ct.created_node_ids.size() + ct.modified_node_ids.size() +
          ct.deleted_node_ids.size());
}

bbox_t ApiDB_Node_Updater::bbox() const { return m_bbox; }
