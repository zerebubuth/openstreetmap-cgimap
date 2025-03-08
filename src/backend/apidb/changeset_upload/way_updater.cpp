/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/http.hpp"
#include "cgimap/options.hpp"
#include "cgimap/util.hpp"

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <fmt/core.h>



ApiDB_Way_Updater::ApiDB_Way_Updater(Transaction_Manager &_m,
                                     const RequestContext& _req_ctx,
                                     api06::OSMChange_Tracking &ct)
  : m(_m),
    req_ctx(_req_ctx),
    ct(ct)
{}

void ApiDB_Way_Updater::add_way(osm_changeset_id_t changeset_id,
                                osm_nwr_signed_id_t old_id,
                                const api06::WayNodeList &nodes,
                                const api06::TagList &tags) {

  way_t new_way{
    .version = 1,
    .changeset_id = changeset_id,
    .old_id = old_id
  };

  for (const auto &[key, value] : tags)
    new_way.tags.emplace_back(key, value);

  // If the following conditions are still not met, although our XML parser
  // raised an exception for it, it's clearly a programming error.
  assert(nodes.size() > 0);
  assert(nodes.size() <= global_settings::get_way_max_nodes());

  osm_sequence_id_t node_seq = 0;
  for (const auto &node : nodes)
    new_way.way_nodes.push_back(
        { (node < 0 ? 0 : static_cast<osm_nwr_id_t>(node)), ++node_seq, node });

  create_ways.push_back(new_way);

  ct.osmchange_orig_sequence.push_back({ operation::op_create,
                                         object_type::way, new_way.old_id,
                                         new_way.version, false });
}

void ApiDB_Way_Updater::modify_way(osm_changeset_id_t changeset_id,
                                   osm_nwr_id_t id, osm_version_t version,
                                   const api06::WayNodeList &nodes,
                                   const api06::TagList &tags) {

  way_t modify_way{
    .id = id,
    .version = version,
    .changeset_id = changeset_id,
    .old_id = static_cast<osm_nwr_signed_id_t>(id)
  };

  for (const auto &[key, value] : tags)
    modify_way.tags.emplace_back(key, value);

  // If the following conditions are still not met, although our XML parser
  // raised an exception for it, it's clearly a programming error.
  assert(nodes.size() > 0);
  assert(nodes.size() <= global_settings::get_way_max_nodes());

  osm_sequence_id_t node_seq = 0;
  for (const auto &node : nodes)
    modify_way.way_nodes.push_back(
        { (node < 0 ? 0 : static_cast<osm_nwr_id_t>(node)), ++node_seq, node });

  modify_ways.push_back(modify_way);

  ct.osmchange_orig_sequence.push_back({ operation::op_modify,
                                         object_type::way, modify_way.old_id,
                                         modify_way.version, false });
}

void ApiDB_Way_Updater::delete_way(osm_changeset_id_t changeset_id,
                                   osm_nwr_id_t id, osm_version_t version,
                                   bool if_unused) {

  way_t delete_way{
    .id = id,
    .version = version,
    .changeset_id = changeset_id,
    .old_id = static_cast<osm_nwr_signed_id_t>(id),
    .if_unused = if_unused
  };

  delete_ways.push_back(delete_way);

  ct.osmchange_orig_sequence.push_back({ operation::op_delete,
                                         object_type::way, delete_way.old_id,
                                         delete_way.version, if_unused });
}

void ApiDB_Way_Updater::process_new_ways() {

  std::vector<osm_nwr_id_t> ids;

  check_unique_placeholder_ids(create_ways);

  insert_new_ways_to_current_table(create_ways);

  // Use new_ids as a result of inserting nodes/ways in tmp table
  replace_old_ids_in_ways(create_ways, ct.created_node_ids,
                          ct.created_way_ids);

  ids.reserve(create_ways.size());

  for (const auto &id : create_ways)
    ids.emplace_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  // lock_current_ways(ids);  // INSERT already set RowExclusiveLock earlier on
  lock_future_nodes(create_ways);

  const auto ids_with_tags = insert_new_current_way_tags(create_ways);
  insert_new_current_way_nodes(create_ways);

  save_current_ways_to_history(ids);
  save_current_way_tags_to_history(ids_with_tags);
  save_current_way_nodes_to_history(ids);

  m_bbox.expand(calc_way_bbox(ids));

  create_ways.clear();
}

void ApiDB_Way_Updater::process_modify_ways() {

  std::vector<osm_nwr_id_t> ids;

  // Use new_ids as a result of inserting nodes/ways in tmp table
  replace_old_ids_in_ways(modify_ways, ct.created_node_ids,
                          ct.created_way_ids);

  for (const auto &id : modify_ways)
    ids.push_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  lock_current_ways(ids);

  // modify may contain several versions of the same way
  // those have to be processed one after the other
  auto packages = build_packages(modify_ways);

  for (const auto &modify_ways_package : packages) {
    std::vector<osm_nwr_id_t> ids_package;

    ids_package.reserve(modify_ways_package.size());

    for (const auto &id : modify_ways_package)
      ids_package.push_back(id.id);

    // remove duplicates
    std::ranges::sort(ids_package);
    auto new_end = std::ranges::unique(ids_package);
    ids_package.erase(new_end.begin(), new_end.end());

    check_current_way_versions(modify_ways_package);

    lock_future_nodes(modify_ways_package);

    m_bbox.expand(calc_way_bbox(ids_package));

    delete_current_way_tags(ids_package);
    delete_current_way_nodes(ids_package);

    update_current_ways(modify_ways_package, true);
    const auto ids_with_tags = insert_new_current_way_tags(modify_ways_package);
    insert_new_current_way_nodes(modify_ways_package);

    save_current_ways_to_history(ids_package);
    save_current_way_tags_to_history(ids_with_tags);
    save_current_way_nodes_to_history(ids_package);

    m_bbox.expand(calc_way_bbox(ids));
  }

  modify_ways.clear();
}

void ApiDB_Way_Updater::process_delete_ways() {

  std::vector<osm_nwr_id_t> ids;

  std::vector<way_t> delete_ways_visible;
  std::vector<osm_nwr_id_t> ids_visible;
  std::vector<osm_nwr_id_t> ids_visible_unreferenced;

  // Use new_ids as a result of inserting nodes/ways in tmp table
  replace_old_ids_in_ways(delete_ways, ct.created_node_ids,
                          ct.created_way_ids);

  for (const auto &id : delete_ways)
    ids.push_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  lock_current_ways(ids);

  // In case the delete element has an "if-unused" flag, we ignore already
  // deleted ways and avoid raising an error message.

  auto already_deleted_ways = determine_already_deleted_ways(delete_ways);

  for (const auto &way : delete_ways)
    if (!already_deleted_ways.contains(way.id)) {
      delete_ways_visible.push_back(way);
      ids_visible.push_back(way.id);
    }

  check_current_way_versions(delete_ways_visible);

  auto delete_ways_visible_unreferenced =
      is_way_still_referenced(delete_ways_visible);

  m_bbox.expand(calc_way_bbox(ids));

  update_current_ways(delete_ways_visible_unreferenced, false);

  ids_visible_unreferenced.reserve(delete_ways_visible_unreferenced.size());

  for (const auto &way : delete_ways_visible_unreferenced)
    ids_visible_unreferenced.push_back(way.id);

  delete_current_way_tags(ids_visible_unreferenced);
  delete_current_way_nodes(ids_visible_unreferenced);

  save_current_ways_to_history(ids_visible_unreferenced);
  // no node tags and way members to save here

  delete_ways.clear();
}

/*
 * Set id field based on old_id -> id mapping
 *
 */
void ApiDB_Way_Updater::replace_old_ids_in_ways(
    std::vector<way_t> &ways,
    const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
        &created_node_id_mapping,
    const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
        &created_way_id_mapping) {
  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map_ways;
  for (auto &i : created_way_id_mapping) {
    auto [_, inserted] = map_ways.insert({ i.old_id, i.new_id });
    if (!inserted)
      throw http::bad_request(
          fmt::format("Duplicate way placeholder id {:d}.", i.old_id));
  }

  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map_nodes;
  for (auto &i : created_node_id_mapping) {
    auto [_, inserted] = map_nodes.insert({ i.old_id, i.new_id });
    if (!inserted)
      throw http::bad_request(
          fmt::format("Duplicate node placeholder id {:d}.", i.old_id));
  }

  for (auto &cw : ways) {
    // TODO: Check if this is possible in case of replace
    if (cw.old_id < 0) {
      auto entry = map_ways.find(cw.old_id);
      if (entry == map_ways.end())
        throw http::bad_request(
            fmt::format("Placeholder id not found for way reference {:d}", cw.old_id));
      cw.id = entry->second;
    }

    for (auto &wn : cw.way_nodes) {
      if (wn.old_node_id < 0) {
        auto entry = map_nodes.find(wn.old_node_id);
        if (entry == map_nodes.end())
          throw http::bad_request(
              fmt::format(
                   "Placeholder node not found for reference {:d} in way {:d}",
               wn.old_node_id, cw.old_id));
        wn.node_id = entry->second;
      }
    }
  }
}

void ApiDB_Way_Updater::check_unique_placeholder_ids(
    const std::vector<way_t> &create_ways) {

  for (const auto &create_way : create_ways) {
    auto [_, inserted] = create_placeholder_ids.insert(create_way.old_id);

    if (!inserted)
      throw http::bad_request(
          "Placeholder IDs must be unique for created elements.");
  }
}

void ApiDB_Way_Updater::insert_new_ways_to_current_table(
    const std::vector<way_t> &create_ways) {

  m.prepare("insert_new_ways_to_current_table", R"(

       WITH ids_mapping AS (
        SELECT nextval('current_ways_id_seq'::regclass) AS id,
               old_id
        FROM
           UNNEST($2::bigint[]) AS id(old_id)
       ),
       tmp_ways AS (
         SELECT
           UNNEST($1::bigint[]) AS changeset_id,
           true::boolean AS visible,
           (now() at time zone 'utc')::timestamp without time zone AS "timestamp",
           1::bigint AS version,
           UNNEST($2::bigint[]) AS old_id
      ),
      insert_op AS (
        INSERT INTO current_ways (id, changeset_id, timestamp, visible, version)
             SELECT id, changeset_id, timestamp, visible, version
             FROM tmp_ways
             INNER JOIN ids_mapping
             ON tmp_ways.old_id = ids_mapping.old_id
      )
      SELECT id, old_id
        FROM ids_mapping
  )"_M);

  std::vector<osm_changeset_id_t> cs;
  std::vector<osm_nwr_signed_id_t> oldids;

  cs.reserve(create_ways.size());
  oldids.reserve(create_ways.size());

  for (const auto &create_way : create_ways) {
    cs.emplace_back(create_way.changeset_id);
    oldids.emplace_back(create_way.old_id);
  }

  auto r = m.exec_prepared("insert_new_ways_to_current_table", cs, oldids);

  if (r.affected_rows() != create_ways.size())
    throw http::server_error("Could not create all new ways in current table");

  const auto old_id_col(r.column_number("old_id"));
  const auto id_col(r.column_number("id"));

  for (const auto &row : r)
    ct.created_way_ids.push_back({ row[old_id_col].as<osm_nwr_signed_id_t>(),
                                    row[id_col].as<osm_nwr_id_t>(), 1 });
}

bbox_t ApiDB_Way_Updater::calc_way_bbox(const std::vector<osm_nwr_id_t> &ids) {

  bbox_t bbox;

  if (ids.empty())
    return bbox;

  m.prepare("calc_way_bbox",
            R"(
      SELECT MIN(latitude)  AS minlat,
             MIN(longitude) AS minlon,
             MAX(latitude)  AS maxlat,
             MAX(longitude) AS maxlon
      FROM current_nodes cn
      INNER JOIN current_way_nodes wn
        ON cn.id = wn.node_id
      INNER JOIN current_ways w
        ON wn.way_id = w.id
      WHERE w.id = ANY($1)
       )"_M);

  auto r = m.exec_prepared("calc_way_bbox", ids);

  if (!(r.empty() || r[0]["minlat"].is_null())) {
    bbox.minlat = r[0]["minlat"].as<int64_t>();
    bbox.minlon = r[0]["minlon"].as<int64_t>();
    bbox.maxlat = r[0]["maxlat"].as<int64_t>();
    bbox.maxlon = r[0]["maxlon"].as<int64_t>();
  }

  return bbox;
}

void ApiDB_Way_Updater::lock_current_ways(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("lock_current_ways", R"(
      WITH locked AS (
        SELECT id FROM current_ways WHERE id = ANY($1) FOR UPDATE
      )
      SELECT t.id FROM UNNEST($1) AS t(id)
      EXCEPT
      SELECT id FROM locked
      ORDER BY id
     )"_M);

  auto r = m.exec_prepared("lock_current_ways", ids);

  if (!r.empty()) {
    std::vector<osm_nwr_id_t> missing_ids;
    missing_ids.reserve(ids.size());

    const auto id_col(r.column_number("id"));

    for (const auto &row : r)
      missing_ids.push_back(row[id_col].as<osm_nwr_id_t>());

    throw http::not_found(
        fmt::format("The following way ids are unknown: {}", to_string(missing_ids)));
  }
}

/*
 * Multiple ways  with the same id cannot be processed in one step but have to
 * be spread across multiple packages
 * which are getting processed one after the other
 *
 */

std::vector<std::vector<ApiDB_Way_Updater::way_t>>
ApiDB_Way_Updater::build_packages(const std::vector<way_t> &ways) {

  std::vector<std::vector<ApiDB_Way_Updater::way_t>> result;

  std::map<osm_nwr_id_t, unsigned int> id_to_package;

  for (const auto &way : ways) {
    if (!id_to_package.contains(way.id))
      id_to_package[way.id] = 0;
    else
      ++id_to_package[way.id];

    if (id_to_package[way.id] + 1 > result.size())
      result.emplace_back();

    result.at(id_to_package[way.id]).emplace_back(way);
  }

  return result;
}

void ApiDB_Way_Updater::check_current_way_versions(
    const std::vector<way_t> &ways) {
  // Assumption: All ways exist on database, and are already locked by
  // lock_current_ways

  if (ways.empty())
    return;

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_version_t> versions;

  for (const auto &w : ways) {
    ids.push_back(w.id);
    versions.push_back(w.version);
  }

  m.prepare("check_current_way_versions",
            R"(
         WITH tmp_way_versions(id, version) AS (
           SELECT * FROM
             UNNEST( CAST($1 as bigint[]),
                     CAST($2 as bigint[])
           )
        )
        SELECT t.id,
              t.version   AS expected_version,
              cw.version  AS actual_version
        FROM tmp_way_versions t
        INNER JOIN current_ways cw
           ON t.id = cw.id
        WHERE t.version <> cw.version
        LIMIT 1
      )"_M);

  auto r =
      m.exec_prepared("check_current_way_versions", ids, versions);

  if (!r.empty()) {
    throw http::conflict(
        fmt::format(
             "Version mismatch: Provided {:d}, server had: {:d} of Way {:d}",
         r[0]["expected_version"].as<osm_version_t>(),
         r[0]["actual_version"].as<osm_version_t>(),
         r[0]["id"].as<osm_nwr_id_t>()));
  }
}

// for if-unused - determine ways to be excluded from deletion, regardless of
// their current version
std::set<osm_nwr_id_t> ApiDB_Way_Updater::determine_already_deleted_ways(
    const std::vector<way_t> &ways) {

  std::set<osm_nwr_id_t> result;

  if (ways.empty())
    return result;

  std::set<osm_nwr_id_t> ids_to_be_deleted;     // all way ids to be deleted
  std::set<osm_nwr_id_t> ids_if_unused;         // delete with if-used flag set
  std::set<osm_nwr_id_t> ids_without_if_unused; // node ids without if-used flag
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  for (const auto &way : ways) {
    ids_to_be_deleted.insert(way.id);
    if (way.if_unused) {
      ids_if_unused.insert(way.id);
    } else {
      ids_without_if_unused.insert(way.id);
    }
    id_to_old_id[way.id] = way.old_id;
  }

  if (ids_to_be_deleted.empty())
    return result;

  m.prepare("already_deleted_ways", "SELECT id, version FROM current_ways "
                                    "WHERE id = ANY($1) AND visible = false");

  auto r = m.exec_prepared("already_deleted_ways", ids_to_be_deleted);

  const auto id_col(r.column_number("id"));
  const auto version_col(r.column_number("version"));

  for (const auto &row : r) {

    auto id = row[id_col].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a way that is already deleted,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.contains(id)) {
      throw http::gone(
          fmt::format("The way with the id {:d} has already been deleted", id));
    }

    result.insert(id);

    // if-used flag is set:
    // We have identified a way that is already deleted on the server. The only
    // thing left to do in this scenario is to return old_id, new_id and the
    // current version to the caller
    if (ids_if_unused.contains(id)) {

      ct.skip_deleted_way_ids.push_back(
          { id_to_old_id[row[id_col].as<osm_nwr_id_t>()],
	    row[id_col].as<osm_nwr_id_t>(),
            row[version_col].as<osm_version_t>() });
    }
  }

  return result;
}

void ApiDB_Way_Updater::lock_future_nodes(const std::vector<way_t> &ways) {

  // Acquire a shared lock on all node ids we want to include in a future
  // current_way_nodes entry
  // All nodes have to be visible to be locked. Deleted nodes trigger an
  // exception

  // !! This method has to be called before inserting new entries in
  // current_way_nodes !!

  std::vector<osm_nwr_id_t> node_ids;

  for (const auto &id : ways) {
    for (const auto &wn : id.way_nodes)
      node_ids.push_back(wn.node_id);
  }

  if (node_ids.empty())
    return; // nothing to do

  // remove duplicates
  std::ranges::sort(node_ids);
  auto new_end = std::ranges::unique(node_ids);
  node_ids.erase(new_end.begin(), new_end.end());

  m.prepare("lock_future_nodes_in_ways",
            R"(
        WITH locked AS (
           SELECT id
           FROM current_nodes
           WHERE visible = true
           AND id = ANY($1) FOR SHARE
        )
        SELECT t.id FROM UNNEST($1) AS t(id)
        EXCEPT
        SELECT id FROM locked
        ORDER BY id
      )"_M);

  auto r = m.exec_prepared("lock_future_nodes_in_ways", node_ids);

  if (!r.empty()) {
    std::set<osm_nwr_id_t> missing_nodes;

    const auto id_col(r.column_number("id"));

    for (const auto &row : r)
      missing_nodes.insert(row[id_col].as<osm_nwr_id_t>());

    std::map<osm_nwr_signed_id_t, std::set<osm_nwr_id_t>> absent_way_node_ids;

    for (const auto &w : ways)
      for (const auto &wn : w.way_nodes)
        if (missing_nodes.contains(wn.node_id))
          absent_way_node_ids[w.old_id].insert(
              wn.node_id); // return node id in osmChange for error msg

    auto it = absent_way_node_ids.begin();

    throw http::precondition_failed(
        fmt::format("Way {:d} requires the nodes with id in {}, which either do not exist, or are not visible.",
         it->first, to_string(it->second)));
  }
}

void ApiDB_Way_Updater::update_current_ways(const std::vector<way_t> &ways,
                                            bool visible) {
  if (ways.empty())
    return;

  m.prepare("update_current_ways", R"(
      WITH u(id, changeset_id, version) AS (
         SELECT * FROM
         UNNEST( CAST($1 AS bigint[]),
                 CAST($2 AS bigint[]),
                 CAST($3 AS bigint[])
              )
      )
      UPDATE current_ways AS w
      SET changeset_id = u.changeset_id,
         visible = CAST($4 as boolean),
         timestamp = (now() at time zone 'utc'),
         version = u.version + 1
         FROM u
      WHERE w.id = u.id
      AND   w.version = u.version
      RETURNING w.id, w.version
     )"_M);

  std::vector<osm_nwr_signed_id_t> ids;
  std::vector<osm_changeset_id_t> cs;
  std::vector<osm_version_t> versions;
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  ids.reserve(ways.size());
  cs.reserve(ways.size());
  versions.reserve(ways.size());

  for (const auto &way : ways) {
    ids.emplace_back(way.id);
    cs.emplace_back(way.changeset_id);
    versions.emplace_back(way.version);
    id_to_old_id[way.id] = way.old_id;
  }

  auto r = m.exec_prepared("update_current_ways", ids, cs, versions, visible);

  if (r.affected_rows() != ways.size())
    throw http::server_error("Could not update all current ways");

  const auto id_col(r.column_number("id"));
  const auto version_col(r.column_number("version"));

  // update modified ways table
  for (const auto &row : r) {
    if (visible)
      ct.modified_way_ids.push_back({id_to_old_id[row[id_col].as<osm_nwr_id_t>()],
                                    row[id_col].as<osm_nwr_id_t>(),
                                    row[version_col].as<osm_version_t>()});
    else
      ct.deleted_way_ids.push_back({ id_to_old_id[row[id_col].as<osm_nwr_id_t>()] });
  }
}

std::vector<osm_nwr_id_t> ApiDB_Way_Updater::insert_new_current_way_tags(
    const std::vector<way_t> &ways) {

 if (ways.empty())
   return {};

#if PQXX_VERSION_MAJOR < 7

  m.prepare("insert_new_current_way_tags",

            R"(
      WITH tmp_tag(way_id, k, v) AS (
         SELECT * FROM
         UNNEST( CAST($1 AS bigint[]),
                 CAST($2 AS character varying[]),
                 CAST($3 AS character varying[])
         )
      )
      INSERT INTO current_way_tags(way_id, k, v)
      SELECT * FROM tmp_tag
     )"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  unsigned total_tags = 0;

  for (const auto &way : ways) {
    for (const auto & [key, value] : way.tags) {
      ids.emplace_back(way.id);
      ks.emplace_back(escape(key));
      vs.emplace_back(escape(value));
      ++total_tags;
    }
  }

  if (total_tags == 0)
    return {};

  auto r = m.exec_prepared("insert_new_current_way_tags", ids, ks, vs);

  if (r.affected_rows() != total_tags)
    throw http::server_error("Could not create new current way tags");

#else

  std::vector<osm_nwr_id_t> ids;

  auto stream = m.to_stream("current_way_tags", "way_id, k, v");

  for (const auto &way : ways) {
    for (const auto &tag : way.tags) {
      stream.write_values(way.id, tag.first, tag.second);
      ids.emplace_back(way.id);
    }
  }

  stream.complete();

#endif

  // prepare list of way ids with tags
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());
  return ids;
}

void ApiDB_Way_Updater::insert_new_current_way_nodes(
    const std::vector<way_t> &ways) {

  if (ways.empty())
    return;

#if PQXX_VERSION_MAJOR < 7

  m.prepare("insert_new_current_way_nodes",

            R"(
      WITH new_way_nodes(way_id, node_id, sequence_id) AS (
         SELECT * FROM
         UNNEST( CAST($1 AS bigint[]),
                 CAST($2 AS bigint[]),
                 CAST($3 AS bigint[])
              )
      )
      INSERT INTO current_way_nodes (way_id, node_id, sequence_id)
      SELECT * FROM new_way_nodes
       )"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_nwr_id_t> nodeids;
  std::vector<osm_sequence_id_t> sequenceids;

  for (const auto &way : ways)
    for (const auto &wn : way.way_nodes) {
      ids.emplace_back(way.id);
      nodeids.emplace_back(wn.node_id);
      sequenceids.emplace_back(wn.sequence_id);
    }

  auto r = m.exec_prepared("insert_new_current_way_nodes", ids, nodeids, sequenceids);
#else

  auto stream = m.to_stream("current_way_nodes", "way_id, node_id, sequence_id");

  for (const auto &way : ways) {
    for (const auto &wn : way.way_nodes) {
      stream.write_values(way.id, wn.node_id, wn.sequence_id);
    }
  }

  stream.complete();

#endif
}

void ApiDB_Way_Updater::save_current_ways_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  // current_ways -> ways

  if (ids.empty())
    return;

  m.prepare("current_ways_to_history",
            R"(
       INSERT INTO ways (way_id, changeset_id, timestamp, version, visible)
        SELECT id, changeset_id, timestamp, version, visible
        FROM current_ways
        WHERE id = ANY($1)
    )"_M);

  auto r = m.exec_prepared("current_ways_to_history", ids);

  if (r.affected_rows() != ids.size())
    throw http::server_error("Could not save current ways to history");
}

void ApiDB_Way_Updater::save_current_way_nodes_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  // current_way_nodes -> way_nodes

  if (ids.empty())
    return;

  m.prepare("current_way_nodes_to_history",
            R"(
   INSERT INTO way_nodes (way_id, node_id, version, sequence_id)
       SELECT  way_id, node_id, version, sequence_id
       FROM current_way_nodes wn
       INNER JOIN current_ways w
       ON wn.way_id = w.id
       WHERE id = ANY($1) )"_M);

  auto r = m.exec_prepared("current_way_nodes_to_history", ids);
}

void ApiDB_Way_Updater::save_current_way_tags_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  // current_way_tags -> way_tags

  if (ids.empty())
    return;

  m.prepare("current_way_tags_to_history",
            R"(
         INSERT INTO way_tags (way_id, k, v, version)
             SELECT way_id, k, v, version
             FROM current_way_tags wt
             INNER JOIN current_ways w
                ON wt.way_id = w.id
             WHERE id = ANY($1)
     )"_M);

  auto r = m.exec_prepared("current_way_tags_to_history", ids);
}

std::vector<ApiDB_Way_Updater::way_t>
ApiDB_Way_Updater::is_way_still_referenced(const std::vector<way_t> &ways) {
  // check if way id is still referenced in relations

  if (ways.empty())
    return ways;

  std::vector<osm_nwr_id_t> ids;
  std::set<osm_nwr_id_t> ids_if_unused; // way ids where if-used flag is set
  std::set<osm_nwr_id_t> ids_without_if_unused; // way ids without if-used flag
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  for (const auto &way : ways) {
    ids.push_back(way.id);
    if (way.if_unused) {
      ids_if_unused.insert(way.id);
    } else {
      ids_without_if_unused.insert(way.id);
    }
    id_to_old_id[way.id] = way.old_id;
  }

  std::vector<way_t> updated_ways = ways;
  std::set<osm_nwr_id_t> ways_to_exclude_from_deletion;

  m.prepare("way_still_referenced_by_relation",
            R"(
      SELECT member_id,
             string_agg(distinct relation_id::text,',') AS relation_ids
         FROM current_relation_members
         WHERE member_type = 'Way'
           AND member_id = ANY($1)
         GROUP BY member_id
      )"_M);

  auto r = m.exec_prepared("way_still_referenced_by_relation", ids);

  for (const auto &row : r) {
    auto way_id = row["member_id"].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a way that is still referenced,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.contains(way_id)) {

      // Without the if-unused, such a situation would lead to an error, and the
      // whole diff upload would fail.
      throw http::precondition_failed(
          fmt::format("Way {:d} is still used by relations {}.",
           row["member_id"].as<osm_nwr_id_t>(),
           row["relation_ids"].c_str()));
    }

    if (ids_if_unused.contains(way_id)) {
      /* a <delete> block in the OsmChange document may have an if-unused
       * attribute
       * If this attribute is present, then the delete operation(s) in this
       * block are conditional and will only be executed if the object to be
       * deleted is not used by another object.
       */

      ways_to_exclude_from_deletion.insert(row["member_id"].as<osm_nwr_id_t>());
    }
  }

  // Prepare updated list of ways, which no longer contains object that are
  // still in use by relations
  // We will simply skip those nodes from now on

  if (!ways_to_exclude_from_deletion.empty()) {

    std::erase_if(updated_ways, [&ways_to_exclude_from_deletion](const way_t &a) {
                         return ways_to_exclude_from_deletion.contains(a.id);
                       });

    // Return old_id, new_id and current version to the caller in case of
    // if-unused, so it's clear that the delete operation was *not* executed,
    // but simply skipped

    m.prepare("still_referenced_ways",
              "SELECT id, version FROM current_ways WHERE id = ANY($1)");

    auto r = m.exec_prepared("still_referenced_ways", ways_to_exclude_from_deletion);

    if (r.affected_rows() != ways_to_exclude_from_deletion.size())
      throw http::server_error(
          "Could not get details about still referenced ways");

    std::set<osm_nwr_id_t> result;

    const auto id_col(r.column_number("id"));
    const auto version_col(r.column_number("version"));

    for (const auto &row : r) {
      result.insert(row[id_col].as<osm_nwr_id_t>());

      // We have identified a node that is still used in a way or relation.
      // However, the caller has indicated via if-unused flag that deletion
      // should not lead to an error. All we can do now is to return old_id,
      // new_id and the current version to the caller

      ct.skip_deleted_way_ids.push_back({
          id_to_old_id[row[id_col].as<osm_nwr_id_t>()],
	        row[id_col].as<osm_nwr_id_t>(),
          row[version_col].as<osm_version_t>()});
    }
  }

  return updated_ways;
}

void ApiDB_Way_Updater::delete_current_way_tags(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("delete_current_way_tags",
            "DELETE FROM current_way_tags WHERE way_id = ANY($1)");

  auto r = m.exec_prepared("delete_current_way_tags", ids);
}

void ApiDB_Way_Updater::delete_current_way_nodes(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("delete_current_way_nodes",
            "DELETE FROM current_way_nodes WHERE way_id = ANY($1)");

  auto r = m.exec_prepared("delete_current_way_nodes", ids);
}

uint32_t ApiDB_Way_Updater::get_num_changes() const {
  return (ct.created_way_ids.size() + ct.modified_way_ids.size() +
          ct.deleted_way_ids.size());
}

bbox_t ApiDB_Way_Updater::bbox() const { return m_bbox; }
