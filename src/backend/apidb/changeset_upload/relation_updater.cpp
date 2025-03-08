/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/http.hpp"
#include "cgimap/util.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <fmt/core.h>



ApiDB_Relation_Updater::ApiDB_Relation_Updater(Transaction_Manager &_m,
                                               const RequestContext& _req_ctx,
                                               api06::OSMChange_Tracking &ct)
  : m(_m),
    req_ctx(_req_ctx),
    ct(ct)
{}

void ApiDB_Relation_Updater::add_relation(osm_changeset_id_t changeset_id,
                                          osm_nwr_signed_id_t old_id,
                                          const RelationMemberList &members,
                                          const TagList &tags) {

  relation_t new_relation{
    .version = 1,
    .changeset_id = changeset_id,
    .old_id = old_id
  };

  for (const auto &[key, value] : tags)
    new_relation.tags.emplace_back(key, value);

  osm_sequence_id_t member_seq = 0;
  for (const auto &member : members) {
    member_t new_member{
      .member_type = member.type(),
      .member_id = static_cast<osm_nwr_id_t>(member.ref() < 0 ? 0 : member.ref()),
      .member_role = member.role(),
      .sequence_id = member_seq++,
      .old_member_id = member.ref()
    };
    new_relation.members.push_back(new_member);
  }

  create_relations.push_back(new_relation);

  ct.osmchange_orig_sequence.push_back(
      { operation::op_create, object_type::relation, new_relation.old_id,
        new_relation.version, false });
}

void ApiDB_Relation_Updater::modify_relation(osm_changeset_id_t changeset_id,
                                             osm_nwr_id_t id,
                                             osm_version_t version,
                                             const RelationMemberList &members,
                                             const TagList &tags) {

  relation_t modify_relation{
    .id = id,
    .version = version,
    .changeset_id = changeset_id,
    .old_id = static_cast<osm_nwr_signed_id_t>(id)
  };

  for (const auto &[key, value] : tags)
    modify_relation.tags.emplace_back(key, value);

  osm_sequence_id_t member_seq = 0;
  for (const auto &member : members) {
    member_t modify_member{
      .member_type = member.type(),
      .member_id = static_cast<osm_nwr_id_t>(member.ref() < 0 ? 0 : member.ref()),
      .member_role = member.role(),
      .sequence_id = member_seq++,
      .old_member_id = member.ref()
    };
    modify_relation.members.push_back(modify_member);
  }

  modify_relations.push_back(modify_relation);

  ct.osmchange_orig_sequence.push_back(
      { operation::op_modify, object_type::relation, modify_relation.old_id,
        modify_relation.version, false });
}

void ApiDB_Relation_Updater::delete_relation(osm_changeset_id_t changeset_id,
                                             osm_nwr_id_t id,
                                             osm_version_t version,
                                             bool if_unused) {

  relation_t delete_relation{
    .id = id,
    .version = version,
    .changeset_id = changeset_id,
    .old_id = static_cast<osm_nwr_signed_id_t>(id),
    .if_unused = if_unused
  };

  delete_relations.push_back(delete_relation);

  ct.osmchange_orig_sequence.push_back(
      { operation::op_delete, object_type::relation, delete_relation.old_id,
        delete_relation.version, if_unused });
}

void ApiDB_Relation_Updater::process_new_relations() {

  check_unique_placeholder_ids(create_relations);
  check_forward_relation_placeholders(create_relations);

  insert_new_relations_to_current_table(create_relations);

  // Use new_ids as a result of inserting nodes/ways in tmp table
  replace_old_ids_in_relations(create_relations, ct.created_node_ids,
                               ct.created_way_ids, ct.created_relation_ids);

  std::vector<osm_nwr_id_t> ids;

  ids.reserve(create_relations.size());

  for (const auto &id : create_relations)
    ids.emplace_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  // lock_current_relations(ids);     // INSERT already set RowExclusiveLock earlier on
  lock_future_members(create_relations, ids);

  const auto ids_with_tags = insert_new_current_relation_tags(create_relations);
  insert_new_current_relation_members(create_relations);

  save_current_relations_to_history(ids);
  save_current_relation_tags_to_history(ids_with_tags);
  save_current_relation_members_to_history(ids);

  m_bbox.expand(calc_relation_bbox(ids));

  create_relations.clear();
}

void ApiDB_Relation_Updater::process_modify_relations() {

  // Use new_ids as a result of inserting nodes/ways/relations in tmp table
  replace_old_ids_in_relations(modify_relations, ct.created_node_ids,
                               ct.created_way_ids, ct.created_relation_ids);

  std::vector<osm_nwr_id_t> ids;

  for (const auto &id : modify_relations)
    ids.push_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  lock_current_relations(ids);

  // modify may contain several versions of the same relation
  // those have to be processed one after the other
  auto packages = build_packages(modify_relations);

  for (const auto &modify_relations_package : packages) {
    std::vector<osm_nwr_id_t> ids_package;

    ids_package.reserve(modify_relations_package.size());

    for (const auto &id : modify_relations_package)
      ids_package.push_back(id.id);

    // remove duplicates
    std::ranges::sort(ids_package);
    auto new_end = std::ranges::unique(ids_package);
    ids_package.erase(new_end.begin(), new_end.end());

    check_current_relation_versions(modify_relations_package);

    lock_future_members(modify_relations_package, ids);

    // Analyse required updates to the bbox before applying changes to the
    // database

    /* rel_ids_bbox_update_full contains all relation ids in the current package
     * where all node & way elements are counted towards a bbox update.
     *
     * According to the Rails port and Wiki, this logic applies in case of:
     *
     * "Adding a relation member or changing tag values causes all node and
     * way members to be added to the bounding box."
     *
     */

    std::vector<osm_nwr_id_t> rel_ids_bbox_update_full;

    {
      auto new_members =
          relations_with_new_relation_members(modify_relations_package);

      auto changed_tags =
          relations_with_changed_relation_tags(modify_relations_package);

      auto rel_full_update_ids = new_members;
      rel_full_update_ids.insert(changed_tags.begin(), changed_tags.end());
      rel_ids_bbox_update_full.assign(rel_full_update_ids.begin(),
                                      rel_full_update_ids.end());
    }

    m_bbox.expand(calc_relation_bbox(rel_ids_bbox_update_full));

    /* The second use case for bbox updates assumes:
     *
     * "Adding or removing nodes or ways from a relation causes them to be
     * added to the changeset bounding box."
     */

    auto rel_ids_bbox_update_partial =
        relations_with_changed_way_node_members(modify_relations_package);

    m_bbox.expand(
        calc_rel_member_difference_bbox(rel_ids_bbox_update_partial, false));

    // We'll continue with the actual database updates

    delete_current_relation_tags(ids_package);
    delete_current_relation_members(ids_package);

    update_current_relations(modify_relations_package, true);
    const auto ids_with_tags = insert_new_current_relation_tags(modify_relations_package);
    insert_new_current_relation_members(modify_relations_package);

    save_current_relations_to_history(ids_package);
    save_current_relation_tags_to_history(ids_with_tags);
    save_current_relation_members_to_history(ids_package);

    /* After the database changes are done, check the updated
     * "current_relation_*" tables again for further bbox updates
     */

    m_bbox.expand(calc_relation_bbox(rel_ids_bbox_update_full));
    m_bbox.expand(
        calc_rel_member_difference_bbox(rel_ids_bbox_update_partial, true));
  }

  modify_relations.clear();
}

void ApiDB_Relation_Updater::process_delete_relations() {

  std::vector<osm_nwr_id_t> ids;

  std::vector<relation_t> delete_relations_visible;
  std::vector<osm_nwr_id_t> ids_visible;
  std::vector<osm_nwr_id_t> ids_visible_unreferenced;

  // Use new_ids as a result of inserting nodes/ways/relations in tmp table
  replace_old_ids_in_relations(delete_relations, ct.created_node_ids,
                               ct.created_way_ids, ct.created_relation_ids);

  for (const auto &id : delete_relations)
    ids.push_back(id.id);

  // remove duplicates
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());

  lock_current_relations(ids);

  // In case the delete element has an "if-unused" flag, we ignore already
  // deleted relations and avoid raising an error message.

  auto already_deleted_relations =
      determine_already_deleted_relations(delete_relations);

  for (const auto &relation : delete_relations)
    if (!already_deleted_relations.contains(relation.id)) {
      delete_relations_visible.push_back(relation);
      ids_visible.push_back(relation.id);
    }

  check_current_relation_versions(delete_relations_visible);

  auto delete_relations_visible_unreferenced =
      is_relation_still_referenced(delete_relations_visible);

  m_bbox.expand(calc_relation_bbox(ids));

  update_current_relations(delete_relations_visible_unreferenced, false);

  ids_visible_unreferenced.reserve(delete_relations_visible_unreferenced.size());

  for (const auto &node : delete_relations_visible_unreferenced)
    ids_visible_unreferenced.push_back(node.id);

  delete_current_relation_tags(ids_visible_unreferenced);
  delete_current_relation_members(ids_visible_unreferenced);

  save_current_relations_to_history(ids_visible_unreferenced);
  // no relation members or relation tags to save here

  delete_relations.clear();
}

/*
 * Set id field based on old_id -> id mapping
 *
 */
void ApiDB_Relation_Updater::replace_old_ids_in_relations(
    std::vector<relation_t> &relations,
    const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
        &created_node_id_mapping,
    const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
        &created_way_id_mapping,
    const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
        &created_relation_id_mapping) {

  // Prepare mapping tables
  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map_relations;
  for (auto &i : created_relation_id_mapping) {
    auto [_, inserted] = map_relations.insert({ i.old_id, i.new_id });
    if (!inserted)
      throw http::bad_request(
          fmt::format("Duplicate relation placeholder id {:d}.", i.old_id));
  }

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

  // replace temporary ids
  for (auto &cr : relations) {
    // TODO: Check if this is possible in case of replace
    if (cr.old_id < 0) {
      auto entry = map_relations.find(cr.old_id);
      if (entry == map_relations.end())
        throw http::bad_request(
            fmt::format(
                 "Placeholder id not found for relation reference {:d}",
             cr.old_id));

      cr.id = entry->second;
    }

    for (auto &mbr : cr.members) {
      if (mbr.old_member_id < 0) {
        if (mbr.member_type == "Node") {
          auto entry = map_nodes.find(mbr.old_member_id);
          if (entry == map_nodes.end())
            throw http::bad_request(
                fmt::format("Placeholder node not found for reference {:d} in relation {:d}",
                 mbr.old_member_id, cr.old_id));
          mbr.member_id = entry->second;
        } else if (mbr.member_type == "Way") {
          auto entry = map_ways.find(mbr.old_member_id);
          if (entry == map_ways.end())
            throw http::bad_request(
                fmt::format("Placeholder way not found for reference {:d} in relation {:d}",
                 mbr.old_member_id, cr.old_id));
          mbr.member_id = entry->second;

        } else if (mbr.member_type == "Relation") {
          auto entry = map_relations.find(mbr.old_member_id);
          if (entry == map_relations.end())
            throw http::bad_request(
                fmt::format("Placeholder relation not found for reference {:d} in relation {:d}",
                 mbr.old_member_id, cr.old_id));
          mbr.member_id = entry->second;
        }
      }
    }
  }
}

void ApiDB_Relation_Updater::check_unique_placeholder_ids(
    const std::vector<relation_t> &create_relations) {

  for (const auto &create_relation : create_relations) {
    auto [_, inserted] = create_placeholder_ids.insert(create_relation.old_id);

    if (!inserted)
      throw http::bad_request(
          "Placeholder IDs must be unique for created elements.");
  }
}

/*
  For compatibility reasons, we don't allow forward references for relation members.

  Some clients might implicitly depend on increasing relation id sequence numbers
  for newly created relations. By forbidding forward references, child relations
  always have to be provided before their respective parent relations.

  See https://github.com/openstreetmap/iD/issues/3208#issuecomment-281942743
  for further discussion

  By adding the current placeholder id at the end of the check, we also
  forbid relation members, which refer to their own relation (recursive
  relation definitions).

*/

void ApiDB_Relation_Updater::check_forward_relation_placeholders(
   const std::vector<relation_t> &create_relations) {

  std::set<osm_nwr_signed_id_t> placeholder_ids;

  for (const auto &cr : create_relations) {
    for (auto &mbr : cr.members) {
      if (mbr.old_member_id < 0 && mbr.member_type == "Relation") {
          auto entry = placeholder_ids.find(mbr.old_member_id);
          if (entry == placeholder_ids.end())
            throw http::bad_request(
                fmt::format("Placeholder relation not found for reference "
                               "{:d} in relation {:d}",
                 mbr.old_member_id, cr.old_id));
      }
    }
    if (cr.old_id < 0) {
      placeholder_ids.insert(cr.old_id);
    }
  }
}


void ApiDB_Relation_Updater::insert_new_relations_to_current_table(
    const std::vector<relation_t> &create_relations) {

  m.prepare("insert_new_relations_to_current_table", R"(

       WITH ids_mapping AS (
        SELECT nextval('current_relations_id_seq'::regclass) AS id,
               old_id
        FROM
           UNNEST($2::bigint[]) AS id(old_id)
       ),
       tmp_relations AS (
         SELECT
           UNNEST($1::bigint[]) AS changeset_id,
           true::boolean AS visible,
           (now() at time zone 'utc')::timestamp without time zone AS "timestamp",
           1::bigint AS version,
           UNNEST($2::bigint[]) AS old_id
      ),
      insert_op AS (
        INSERT INTO current_relations (id, changeset_id, timestamp, visible, version)
             SELECT id, changeset_id, timestamp, visible, version
             FROM tmp_relations
             INNER JOIN ids_mapping
             ON tmp_relations.old_id = ids_mapping.old_id
      )
      SELECT id, old_id
        FROM ids_mapping
    )"_M);

  std::vector<osm_changeset_id_t> cs;
  std::vector<osm_nwr_signed_id_t> oldids;

  cs.reserve(create_relations.size());
  oldids.reserve(create_relations.size());

  for (const auto &create_relation : create_relations) {
    cs.emplace_back(create_relation.changeset_id);
    oldids.emplace_back(create_relation.old_id);
  }

  auto r = m.exec_prepared("insert_new_relations_to_current_table", cs, oldids);

  if (r.affected_rows() != create_relations.size())
    throw http::server_error(
        "Could not create all new relations in current table");

  const auto old_id_col(r.column_number("old_id"));
  const auto id_col(r.column_number("id"));

  for (const auto &row : r) {
    ct.created_relation_ids.push_back(
        { row[old_id_col].as<osm_nwr_signed_id_t>(),
          row[id_col].as<osm_nwr_id_t>(),
          1 });
  }
}

void ApiDB_Relation_Updater::lock_current_relations(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("lock_current_relations", R"(
      WITH locked AS (
        SELECT id FROM current_relations WHERE id = ANY($1) FOR UPDATE
      )
      SELECT t.id FROM UNNEST($1) AS t(id)
      EXCEPT
      SELECT id FROM locked
      ORDER BY id
     )"_M);

  auto r = m.exec_prepared("lock_current_relations", ids);

  if (!r.empty()) {
    std::vector<osm_nwr_id_t> missing_ids;
    missing_ids.reserve(ids.size());

    const auto id_col(r.column_number("id"));

    for (const auto &row : r)
      missing_ids.push_back(row[id_col].as<osm_nwr_id_t>());

    throw http::not_found(
        fmt::format("The following relation ids are unknown: {}", to_string(missing_ids)));
  }
}

/*
 * Multiple relations with the same id cannot be processed in one step but have
 * to be spread across multiple packages
 * which are getting processed one after the other
 *
 */

std::vector<std::vector<ApiDB_Relation_Updater::relation_t>>
ApiDB_Relation_Updater::build_packages(
    const std::vector<relation_t> &relations) {

  std::vector<std::vector<ApiDB_Relation_Updater::relation_t>> result;

  std::map<osm_nwr_id_t, unsigned int> id_to_package;

  for (const auto &relation : relations) {
    if (!id_to_package.contains(relation.id))
      id_to_package[relation.id] = 0;
    else
      ++id_to_package[relation.id];

    if (id_to_package[relation.id] + 1 > result.size())
      result.emplace_back();

    result.at(id_to_package[relation.id]).emplace_back(relation);
  }

  return result;
}

void ApiDB_Relation_Updater::check_current_relation_versions(
    const std::vector<relation_t> &relations) {
  // Assumption: All nodes exist on database, and are already locked by
  // lock_current_nodes

  if (relations.empty())
    return;

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_version_t> versions;

  for (const auto &r : relations) {
    ids.push_back(r.id);
    versions.push_back(r.version);
  }

  m.prepare("check_current_relation_versions",
            R"(   WITH tmp_relation_versions(id, version) AS (
                  SELECT * FROM
                       UNNEST( CAST($1 as bigint[]),
                               CAST($2 as bigint[]))
                  )
                  SELECT t.id,
                         t.version  AS expected_version,
                         cr.version AS actual_version
                  FROM tmp_relation_versions t
                  INNER JOIN current_relations cr
                     ON t.id = cr.id
                  WHERE t.version <> cr.version
                  LIMIT 1
       )"_M);

  auto r = m.exec_prepared("check_current_relation_versions", ids, versions);

  if (!r.empty()) {
    throw http::conflict(fmt::format("Version mismatch: Provided {:d}, server had: {:d} of Relation {:d}",
                          r[0]["expected_version"].as<osm_version_t>(),
                          r[0]["actual_version"].as<osm_version_t>(),
                          r[0]["id"].as<osm_nwr_id_t>()));
  }
}

// for if-unused - determine ways to be excluded from deletion, regardless of
// their current version
std::set<osm_nwr_id_t>
ApiDB_Relation_Updater::determine_already_deleted_relations(
    const std::vector<relation_t> &relations) {

  std::set<osm_nwr_id_t> result;

  if (relations.empty())
    return result;

  std::set<osm_nwr_id_t> ids_to_be_deleted; // all relation ids to be deleted
  std::set<osm_nwr_id_t> ids_if_unused;     // delete with if-used flag set
  std::set<osm_nwr_id_t> ids_without_if_unused; // node ids without if-used flag
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  for (const auto &relation : relations) {
    ids_to_be_deleted.insert(relation.id);
    if (relation.if_unused) {
      ids_if_unused.insert(relation.id);
    } else {
      ids_without_if_unused.insert(relation.id);
    }
    id_to_old_id[relation.id] = relation.old_id;
  }

  if (ids_to_be_deleted.empty())
    return result;

  m.prepare("already_deleted_relations", "SELECT id, version FROM "
                                         "current_relations WHERE id = ANY($1) "
                                         "AND visible = false");

  auto r =
      m.exec_prepared("already_deleted_relations", ids_to_be_deleted);

  for (const auto &row : r) {
    osm_nwr_id_t id = row["id"].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a relation that is already deleted,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.contains(id)) {
      throw http::gone(
          fmt::format("The relation with the id {:d} has already been deleted", id));
    }

    result.insert(id);

    // if-used flag is set:
    // We have identified a relation that is already deleted on the server. The
    // only thing left to do in this scenario is to return old_id, new_id and
    // the current version to the caller

    if (ids_if_unused.contains(id)) {

      ct.skip_deleted_relation_ids.push_back(
          { id_to_old_id[row["id"].as<osm_nwr_id_t>()],
	    row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>() });
    }
  }

  return result;
}

void ApiDB_Relation_Updater::lock_future_members_nodes(
    std::vector< osm_nwr_id_t >& node_ids,
    const std::vector< relation_t > &relations)
{
  if (node_ids.empty())
    return;

  std::ranges::sort(node_ids);
  auto new_end = std::ranges::unique(node_ids);
  node_ids.erase(new_end.begin(), new_end.end());

  m.prepare("lock_future_nodes_in_relations",
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

  auto r = m.exec_prepared("lock_future_nodes_in_relations", node_ids);

  if (!r.empty()) {
    std::set<osm_nwr_id_t> missing_nodes;

    for (const auto &row : r)
      missing_nodes.insert(row["id"].as<osm_nwr_id_t>());

    std::map<osm_nwr_signed_id_t, std::set<osm_nwr_id_t>> absent_rel_node_ids;

    for (const auto &rel : relations)
      for (const auto &rm : rel.members)
        if (rm.member_type == "Node" &&
            missing_nodes.contains(rm.member_id))
          absent_rel_node_ids[rel.old_id].insert(
              rm.member_id); // return rel id in osmChange for error msg

    auto it = absent_rel_node_ids.begin();

    throw http::precondition_failed(
        fmt::format("Relation {:d} requires the nodes with id in {}, "
                       "which either do not exist, or are not visible.",
         it->first, to_string(it->second)));
  }
}

void ApiDB_Relation_Updater::lock_future_members_ways(
    std::vector< osm_nwr_id_t >& way_ids,
    const std::vector< relation_t > &relations)
{
  if (way_ids.empty())
    return;

  std::ranges::sort(way_ids);
  auto new_end = std::ranges::unique(way_ids);
  way_ids.erase(new_end.begin(), new_end.end());

  m.prepare("lock_future_ways_in_relations",
            R"(
              WITH locked AS (
                SELECT id
                FROM current_ways
                WHERE visible = true
                AND id = ANY($1) FOR SHARE
              )
              SELECT t.id FROM UNNEST($1) AS t(id)
              EXCEPT
              SELECT id FROM locked
              ORDER BY id
           )"_M);

  auto r = m.exec_prepared("lock_future_ways_in_relations", way_ids);

  if (!r.empty()) {
    std::set<osm_nwr_id_t> missing_nodes;

    const auto id_col(r.column_number("id"));

    for (const auto &row : r)
      missing_nodes.insert(row[id_col].as<osm_nwr_id_t>());

    std::map<osm_nwr_signed_id_t, std::set<osm_nwr_id_t>> absent_rel_way_ids;

    for (const auto &rel : relations)
      for (const auto &rm : rel.members)
        if (rm.member_type == "Way" &&
            missing_nodes.contains(rm.member_id))
          absent_rel_way_ids[rel.old_id].insert(
              rm.member_id); // return rel id in osmChange for error msg

    auto it = absent_rel_way_ids.begin();

    throw http::precondition_failed(
        fmt::format("Relation {:d} requires the ways with id in {}, which "
                       "either do not exist, or are not visible.",
         it->first, to_string(it->second)));
  }
}


void ApiDB_Relation_Updater::lock_future_members_relations(
    std::vector< osm_nwr_id_t >& relation_ids,
    const std::vector< relation_t > &relations)
{
  if (relation_ids.empty())
    return;

  std::ranges::sort(relation_ids);
  auto new_end = std::ranges::unique(relation_ids);
  relation_ids.erase(new_end.begin(), new_end.end());

  m.prepare("lock_future_relations_in_relations",
            R"(
              WITH locked AS (
                SELECT id
                FROM current_relations
                WHERE visible = true
                AND id = ANY($1) FOR SHARE
              )
              SELECT t.id FROM UNNEST($1) AS t(id)
              EXCEPT
              SELECT id FROM locked
              ORDER BY id
            )"_M);

  auto r = m.exec_prepared("lock_future_relations_in_relations", relation_ids);

  if (!r.empty()) {
    std::set<osm_nwr_id_t> missing_nodes;

    const auto id_col(r.column_number("id"));

    for (const auto &row : r)
      missing_nodes.insert(row[id_col].as<osm_nwr_id_t>());

    std::map<osm_nwr_signed_id_t, std::set<osm_nwr_id_t>> absent_rel_rel_ids;

    for (const auto &rel : relations)
      for (const auto &rm : rel.members)
        if (rm.member_type == "Relation" &&
            missing_nodes.contains(rm.member_id))
          absent_rel_rel_ids[rel.old_id].insert(
              rm.member_id); // return rel id in osmChange for error msg

    auto it = absent_rel_rel_ids.begin();

    throw http::precondition_failed(
        fmt::format("Relation {:d} requires the relations with id in {}, "
                       "which either do not exist, or are not visible.",
         it->first, to_string(it->second)));
  }
}

void ApiDB_Relation_Updater::lock_future_members(
    const std::vector<relation_t> &relations,
    const std::vector<osm_nwr_id_t>& already_locked_relations) {

  // Ids for Shared Locking
  std::vector<osm_nwr_id_t> node_ids;
  std::vector<osm_nwr_id_t> way_ids;
  std::vector<osm_nwr_id_t> relation_ids;

  for (const auto &id : relations) {
    for (const auto &rm : id.members) {
      if (rm.member_type == "Node")
        node_ids.push_back(rm.member_id);
      else if (rm.member_type == "Way")
        way_ids.push_back(rm.member_id);
      else if (rm.member_type == "Relation") {

        /*  Only lock relations which haven't been previously locked by lock_current_relations.
         *  Although, lock_current_relations did not check if those relations are visible,
         *  we only lock future members in case of creating or modifying relations.
         *  Newly created relations are visible by default (v1), modified relations will
         *  be set to visible by update_current_relations before committing.
         */

        if (std::ranges::find(already_locked_relations, 
                              rm.member_id) == already_locked_relations.end()) {
          relation_ids.push_back(rm.member_id);
        }
      }
    }
  }

  if (node_ids.empty() && way_ids.empty() && relation_ids.empty())
    return; // nothing to do

  lock_future_members_nodes(node_ids, relations);
  lock_future_members_ways(way_ids, relations);
  lock_future_members_relations(relation_ids, relations);
}

// Helper for bbox calculation: Adding a relation member causes all node and
// way members to be added to the bounding box.
// Note: This method has to be run BEFORE updating the current_relation tables,
// as it compares the future state in a temporary structure with the state
// before the database update

std::set<osm_nwr_id_t>
ApiDB_Relation_Updater::relations_with_new_relation_members(
    const std::vector<relation_t> &relations) {
  std::set<osm_nwr_id_t> result;

  if (relations.empty())
    return result;

  std::vector<osm_nwr_id_t> relation_ids;
  std::vector<osm_nwr_id_t> member_ids;

  for (const auto &r : relations) {
    for (const auto &rm : r.members) {
      if (rm.member_type == "Relation") {
        relation_ids.push_back(r.id);
        member_ids.push_back(rm.member_id);
      }
    }
  }

  m.prepare("relations_with_new_relation_members",
            R"(
          WITH tmp_relation_members(relation_id, member_id) AS
             ( SELECT * FROM
                  UNNEST( CAST($1 as bigint[]),
                          CAST($2 as bigint[])
             )
          )
          SELECT t.relation_id
          FROM   tmp_relation_members t
          LEFT OUTER JOIN current_relation_members m
            ON t.relation_id = m.relation_id
           AND t.member_id   = m.member_id
           AND m.member_type = 'Relation'
          WHERE m.member_id IS NULL
          GROUP BY t.relation_id
     )"_M);

  auto r = m.exec_prepared("relations_with_new_relation_members", relation_ids, member_ids);

  for (const auto &row : r) {
    result.insert(row["relation_id"].as<osm_nwr_id_t>());
  }

  return result;
}

// Helper for bbox calculation: Changing tag value causes all node and
// way members to be added to the bounding box.
// Note: This method has to be run BEFORE updating the current_relation tables,
// as it compares the future state in a temporary structure with the state
// before the database update

std::set<osm_nwr_id_t>
ApiDB_Relation_Updater::relations_with_changed_relation_tags(
    const std::vector<relation_t> &relations) {

  std::set<osm_nwr_id_t> result;

  if (relations.empty())
    return result;

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  for (const auto &relation : relations)
    for (const auto &[key, value] : relation.tags) {
      ids.push_back(relation.id);
      ks.push_back(escape(key));
      vs.push_back(escape(value));
    }

  m.prepare("relations_with_changed_relation_tags",
            R"(
            WITH tmp_relation_tags(relation_id, k, v) AS
                 ( SELECT * FROM
                      UNNEST( CAST($1 as bigint[]),
                              CAST($2 AS character varying[]),
                              CAST($3 AS character varying[])
                 )
            )
            SELECT all_relations.relation_id FROM (
              /* new tag was added in tmp */
              SELECT t.relation_id
                FROM   tmp_relation_tags t
                  LEFT OUTER JOIN current_relation_tags c
                     ON t.relation_id = c.relation_id
                    AND t.k = c.k
                    AND t.v = c.v
                 WHERE c.k IS NULL
                   AND c.v IS NULL
              UNION ALL
                /* existing tag was removed in tmp */
                SELECT c.relation_id
                   FROM current_relation_tags c
                   INNER JOIN tmp_relation_tags t1
                     ON c.relation_id = t1.relation_id
                   LEFT OUTER JOIN tmp_relation_tags t
                      ON c.relation_id = t.relation_id
                     AND c.k = t.k
                     AND c.v = t.v
                WHERE t.k IS NULL
                  AND t.v IS NULL
            ) AS all_relations
            GROUP BY all_relations.relation_id
         )"_M);

  auto r = m.exec_prepared("relations_with_changed_relation_tags", ids, ks, vs);

  for (const auto &row : r) {
    result.insert(row["relation_id"].as<osm_nwr_id_t>());
  }

  return result;
}

// Helper for bbox calculation: Adding or removing nodes or ways from a relation
// causes them to be added to the changeset bounding box.
// Note: This method has to be run BEFORE updating the current_relation tables,
// as it compares the future state in a temporary structure with the state
// before the database update

std::vector<ApiDB_Relation_Updater::rel_member_difference_t>
ApiDB_Relation_Updater::relations_with_changed_way_node_members(
    const std::vector<relation_t> &relations) {

  std::vector<rel_member_difference_t> result;

  if (relations.empty())
    return result;

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> membertypes;
  std::vector<osm_nwr_id_t> memberids;

  for (const auto &relation : relations)
    for (const auto &member : relation.members) {
      if (member.member_type == "Node" || member.member_type == "Way") {

        ids.push_back(relation.id);
        membertypes.push_back(member.member_type);
        memberids.push_back(member.member_id);
      }
    }

  // new member was added in tmp
  m.prepare("relations_with_added_way_node_members",
            R"(
            WITH tmp_member(relation_id, member_type, member_id) AS (
                 SELECT * FROM
                 UNNEST( CAST($1 as bigint[]),
                         CAST($2 as nwr_enum[]),
                         CAST($3 as bigint[])
                 )
              )
            SELECT tm.member_type, tm.member_id, true as new_member
                   FROM tmp_member tm
                   LEFT OUTER JOIN current_relation_members cm
                     ON tm.relation_id = cm.relation_id
                    AND tm.member_type = cm.member_type
                    AND tm.member_id   = cm.member_id
                 WHERE cm.relation_id IS NULL AND
                       cm.member_type IS NULL AND
                       cm.member_id   IS NULL
         )"_M);

  auto r_added = m.exec_prepared("relations_with_added_way_node_members", ids, membertypes, memberids);

  for (const auto &row : r_added) {
    rel_member_difference_t diff;
    diff.member_type = row["member_type"].as<std::string>();
    diff.member_id = row["member_id"].as<osm_nwr_id_t>();
    diff.new_member = row["new_member"].as<bool>();
    result.push_back(diff);
  }

  // existing member was removed in tmp
  m.prepare("relations_with_removed_way_node_members",
            R"(
            WITH tmp_member(relation_id, member_type, member_id) AS (
                 SELECT * FROM
                 UNNEST( CAST($1 as bigint[]),
                         CAST($2 as nwr_enum[]),
                         CAST($3 as bigint[])
                 )
              )
            SELECT cm.member_type, cm.member_id, false as new_member
               FROM current_relation_members cm
               LEFT OUTER JOIN tmp_member tm
                  ON cm.relation_id = tm.relation_id
                 AND cm.member_type = tm.member_type
                 AND cm.member_id   = tm.member_id
            WHERE cm.relation_id IN (SELECT DISTINCT relation_id FROM tmp_member) AND
                  tm.relation_id IS NULL AND
                  tm.member_type IS NULL AND
                  tm.member_id   IS NULL

         )"_M);

  auto r_removed = m.exec_prepared("relations_with_removed_way_node_members", ids, membertypes, memberids);

  for (const auto &row : r_removed) {
    rel_member_difference_t diff;
    diff.member_type = row["member_type"].as<std::string>();
    diff.member_id = row["member_id"].as<osm_nwr_id_t>();
    diff.new_member = row["new_member"].as<bool>();
    result.push_back(diff);
  }

  return result;
}

bbox_t ApiDB_Relation_Updater::calc_rel_member_difference_bbox(
    std::vector<ApiDB_Relation_Updater::rel_member_difference_t> &diff,
    bool process_new_elements) {

  bbox_t result;

  if (diff.empty())
    return result;

  std::vector<osm_nwr_id_t> node_ids;
  std::vector<osm_nwr_id_t> way_ids;

  for (const auto &d : diff) {
    if (d.new_member == process_new_elements) {
      if (d.member_type == "Node")
        node_ids.push_back(d.member_id);
      else if (d.member_type == "Way")
        way_ids.push_back(d.member_id);
    }
  }

  if (!node_ids.empty()) {

    bbox_t bbox_nodes;

    m.prepare("calc_node_bbox_rel_member",
              R"(
      SELECT MIN(latitude)  AS minlat,
             MIN(longitude) AS minlon,
             MAX(latitude)  AS maxlat,
             MAX(longitude) AS maxlon
      FROM current_nodes WHERE id = ANY($1)
       )"_M);

    auto r = m.exec_prepared("calc_node_bbox_rel_member", node_ids);

    if (!(r.empty() || r[0]["minlat"].is_null())) {
      bbox_nodes.minlat = r[0]["minlat"].as<int64_t>();
      bbox_nodes.minlon = r[0]["minlon"].as<int64_t>();
      bbox_nodes.maxlat = r[0]["maxlat"].as<int64_t>();
      bbox_nodes.maxlon = r[0]["maxlon"].as<int64_t>();
    }

    result.expand(bbox_nodes);
  }

  if (!way_ids.empty()) {

    bbox_t bbox_ways;

    m.prepare("calc_way_bbox_rel_member",
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

    auto r = m.exec_prepared("calc_way_bbox_rel_member", way_ids);

    if (!(r.empty() || r[0]["minlat"].is_null())) {
      bbox_ways.minlat = r[0]["minlat"].as<int64_t>();
      bbox_ways.minlon = r[0]["minlon"].as<int64_t>();
      bbox_ways.maxlat = r[0]["maxlat"].as<int64_t>();
      bbox_ways.maxlon = r[0]["maxlon"].as<int64_t>();
    }

    result.expand(bbox_ways);
  }

  return result;
}

bbox_t ApiDB_Relation_Updater::calc_relation_bbox(
    const std::vector<osm_nwr_id_t> &ids) {

  bbox_t bbox;

  /*
   *
   *  Relations:
   *
   *  - Adding or removing nodes or ways from a relation causes them to be
   * added to the changeset bounding box.
   *
   *  - Adding a relation member or changing tag values causes all node and
   * way members to be added to the bounding box.
   *
   *
   *  This is similar to how the map call does things and is reasonable on
   *  the assumption that adding or removing members doesn't materially
   *   change the rest of the relation.
   *
   */

  if (ids.empty())
    return bbox;

  m.prepare("calc_relation_bbox_nodes",
            R"(
                SELECT MIN(latitude)  AS minlat,
                       MIN(longitude) AS minlon,
                       MAX(latitude)  AS maxlat,
                       MAX(longitude) AS maxlon
                FROM current_nodes cn
                INNER JOIN current_relation_members crm
                        ON crm.member_id = cn.id
                 WHERE crm.member_type = 'Node'
                   AND crm.relation_id = ANY($1)
            )"_M);

  auto rn = m.exec_prepared("calc_relation_bbox_nodes", ids);

  if (!(rn.empty() || rn[0]["minlat"].is_null())) {
    bbox.minlat = rn[0]["minlat"].as<int64_t>();
    bbox.minlon = rn[0]["minlon"].as<int64_t>();
    bbox.maxlat = rn[0]["maxlat"].as<int64_t>();
    bbox.maxlon = rn[0]["maxlon"].as<int64_t>();
  }

  m.prepare("calc_relation_bbox_ways",
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
                INNER JOIN current_relation_members crm
                        ON crm.member_id = w.id
                 WHERE crm.member_type = 'Way'
                   AND crm.relation_id = ANY($1)
              )"_M);

  auto rw = m.exec_prepared("calc_relation_bbox_ways", ids);

  if (!(rw.empty() || rw[0]["minlat"].is_null())) {
    bbox_t bbox_way;

    bbox_way.minlat = rw[0]["minlat"].as<int64_t>();
    bbox_way.minlon = rw[0]["minlon"].as<int64_t>();
    bbox_way.maxlat = rw[0]["maxlat"].as<int64_t>();
    bbox_way.maxlon = rw[0]["maxlon"].as<int64_t>();
    bbox.expand(bbox_way);
  }

  return bbox;
}

void ApiDB_Relation_Updater::update_current_relations(
    const std::vector<relation_t> &relations, bool visible) {
  if (relations.empty())
    return;

  m.prepare("update_current_relations",
            R"(
        WITH u(id, changeset_id, version) AS (
                SELECT * FROM
                UNNEST( CAST($1 AS bigint[]),
                        CAST($2 AS bigint[]),
                        CAST($3 AS bigint[])
                      )
        )
        UPDATE current_relations AS r
        SET changeset_id = u.changeset_id,
            visible = CAST($4 as boolean),
            timestamp = (now() at time zone 'utc'),
            version = u.version + 1
        FROM u
        WHERE r.id = u.id
          AND r.version = u.version
        RETURNING r.id, r.version
    )"_M);

  std::vector<osm_nwr_signed_id_t> ids;
  std::vector<osm_changeset_id_t> cs;
  std::vector<osm_version_t> versions;
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  ids.reserve(relations.size());
  cs.reserve(relations.size());
  versions.reserve(relations.size());

  for (const auto &relation : relations) {
    ids.emplace_back(relation.id);
    cs.emplace_back(relation.changeset_id);
    versions.emplace_back(relation.version);
    id_to_old_id[relation.id] = relation.old_id;
  }

  auto r = m.exec_prepared("update_current_relations", ids, cs, versions, visible);

  if (r.affected_rows() != relations.size())
    throw http::server_error("Could not update all current relations");

  // update modified/deleted relations table
  for (const auto &row : r) {
    if (visible) {
      ct.modified_relation_ids.push_back(
          { id_to_old_id[row["id"].as<osm_nwr_id_t>()],
	          row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>() });
    } else {
      ct.deleted_relation_ids.push_back({ id_to_old_id[row["id"].as<osm_nwr_id_t>()] });
    }
  }
}

std::vector<osm_nwr_id_t>  ApiDB_Relation_Updater::insert_new_current_relation_tags(
    const std::vector<relation_t> &relations) {

  if (relations.empty())
    return {};

#if PQXX_VERSION_MAJOR < 7

  m.prepare("insert_new_current_relation_tags",

            R"(
                WITH tmp_tag(relation_id, k, v) AS (
                   SELECT * FROM
                   UNNEST( CAST($1 AS bigint[]),
                           CAST($2 AS character varying[]),
                           CAST($3 AS character varying[])
                   )
                )
                INSERT INTO current_relation_tags(relation_id, k, v)
                SELECT * FROM tmp_tag
            )"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  unsigned total_tags = 0;

  for (const auto &relation : relations) {
    for (const auto &[key, value] : relation.tags) {
      ids.emplace_back(relation.id);
      ks.emplace_back(escape(key));
      vs.emplace_back(escape(value));
      ++total_tags;
    }
  }

  if (total_tags == 0)
    return {};

  auto r = m.exec_prepared("insert_new_current_relation_tags", ids, ks, vs);

  if (r.affected_rows() != total_tags)
    throw http::server_error("Could not create new current relation tags");


#else

  std::vector<osm_nwr_id_t> ids;

  auto stream = m.to_stream("current_relation_tags", "relation_id, k, v");

  for (const auto &relation : relations) {
    for (const auto &tag : relation.tags) {
      stream.write_values(relation.id, tag.first, tag.second);
      ids.emplace_back(relation.id);
    }
  }

  stream.complete();

#endif

  // prepare list of relation ids with tags
  std::ranges::sort(ids);
  auto new_end = std::ranges::unique(ids);
  ids.erase(new_end.begin(), new_end.end());
  return ids;

}

void ApiDB_Relation_Updater::insert_new_current_relation_members(
    const std::vector<relation_t> &relations) {

  if (relations.empty())
    return;

#if PQXX_VERSION_MAJOR < 7

  m.prepare("insert_new_current_relation_members",

            R"(
         WITH tmp_member(relation_id, member_type, member_id, member_role, sequence_id) AS (
                 SELECT * FROM
                 UNNEST( CAST($1 as bigint[]),
                         CAST($2 as nwr_enum[]),
                         CAST($3 as bigint[]),
                         CAST($4 as character varying[]),
                         CAST($5 as integer[])
                 )
         )
         INSERT INTO current_relation_members(relation_id, member_type, member_id, member_role, sequence_id)
         SELECT * FROM tmp_member
      )"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> membertypes;
  std::vector<osm_nwr_id_t> memberids;
  std::vector<std::string> memberroles;
  std::vector<osm_sequence_id_t> sequenceids;

  for (const auto &relation : relations)
    for (const auto &member : relation.members) {
      ids.emplace_back(relation.id);
      membertypes.emplace_back(member.member_type);
      memberids.emplace_back(member.member_id);
      memberroles.emplace_back(escape(member.member_role));
      sequenceids.emplace_back(member.sequence_id);
    }

  auto r = m.exec_prepared("insert_new_current_relation_members",
				   ids, membertypes, memberids, memberroles, sequenceids);
#else

  auto stream = m.to_stream("current_relation_members", "relation_id, member_type, member_id, member_role, sequence_id");

  for (const auto &relation : relations) {
    for (const auto &member : relation.members) {
      stream.write_values(relation.id, member.member_type, member.member_id, member.member_role, member.sequence_id);
    }
  }

  stream.complete();

#endif
}

void ApiDB_Relation_Updater::save_current_relations_to_history(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("current_relations_to_history",
            R"(
                INSERT INTO relations (relation_id, changeset_id, timestamp, version, visible)
                SELECT id AS relation_id, changeset_id, timestamp, version, visible
                FROM current_relations
                WHERE id = ANY($1)
            )"_M);

  auto r = m.exec_prepared("current_relations_to_history", ids);

  if (r.affected_rows() != ids.size())
    throw http::server_error("Could not save current relations to history");
}

void ApiDB_Relation_Updater::save_current_relation_tags_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return;

  m.prepare("current_relation_tags_to_history",
            R"(
                INSERT INTO relation_tags (relation_id, k, v, version)
                 SELECT relation_id, k, v, version FROM current_relation_tags rt
                 INNER JOIN current_relations cr
                 ON rt.relation_id = cr.id
                 WHERE id = ANY($1)
             )"_M);

  auto r = m.exec_prepared("current_relation_tags_to_history", ids);
}

void ApiDB_Relation_Updater::save_current_relation_members_to_history(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("current_relation_members_to_history",
            R"(
                INSERT INTO relation_members (relation_id, member_type, member_id, member_role,
                        version, sequence_id)
                 SELECT relation_id, member_type, member_id, member_role,
                        version, sequence_id
                 FROM current_relation_members crm
                 INNER JOIN current_relations cr
                 ON crm.relation_id = cr.id
                 WHERE id = ANY($1)
                          )"_M);

  auto r =
      m.exec_prepared("current_relation_members_to_history", ids);
}

void
ApiDB_Relation_Updater::remove_blocked_relations_from_deletion_list (
    std::set<osm_nwr_id_t> relations_to_exclude_from_deletion,
    std::map<osm_nwr_id_t, osm_nwr_signed_id_t> &id_to_old_id,
    std::vector<relation_t> &updated_relations)
{
  // Prepare updated list of relations, which no longer contains objects that are
  // still in use by relations. We will simply skip those relations from now on

  if (relations_to_exclude_from_deletion.empty())
    return;

  std::erase_if(updated_relations, [&](const relation_t &a) {
				    return relations_to_exclude_from_deletion.contains(a.id); });

  // Return old_id, new_id and current version to the caller in case of
  // if-unused, so it's clear that the delete operation was *not* executed,
  // but simply skipped
  m.prepare("still_referenced_relations", "SELECT id, version FROM current_relations WHERE id = ANY($1)");

  auto r = m.exec_prepared ("still_referenced_relations", relations_to_exclude_from_deletion);

  if (r.affected_rows() != relations_to_exclude_from_deletion.size())
    throw http::server_error (
	"Could not get details about still referenced relations");

  for (const auto &row : r) {
    // We have identified a node that is still used in a way or relation.
    // However, the caller has indicated via if-unused flag that deletion
    // should not lead to an error. All we can do now is to return old_id,
    // new_id and the current version to the caller
    ct.skip_deleted_relation_ids.push_back(
	  { id_to_old_id[row["id"].as<osm_nwr_id_t>()],
	    row["id"].as< osm_nwr_id_t>(),
	    row["version"].as<osm_version_t>() });
  }
}

std::set<osm_nwr_id_t>
ApiDB_Relation_Updater::collect_recursive_relation_rel_member_ids (
    const std::set<osm_nwr_id_t> &direct_relation_ids)
{
  // Calculate transitive closure of all relation ids' relation member children

  std::set<osm_nwr_id_t> calc_relation_children_ids; // list of relation ids for which member relations are to be extracted
  std::set<osm_nwr_id_t> transitive_relation_children_ids; // transitive list of all relation members

  if (direct_relation_ids.empty())
    return transitive_relation_children_ids;

  calc_relation_children_ids = direct_relation_ids;

  m.prepare("calc_child_relation_ids_for_relation_ids",
      R"(
           WITH relations_to_check (id) AS (
              SELECT * FROM
                UNNEST( CAST($1 AS bigint[]) )
           )
           SELECT DISTINCT crm.member_id
           FROM current_relations cr
             INNER JOIN relations_to_check c
                     ON cr.id = c.id
             INNER JOIN current_relation_members crm
                     ON crm.relation_id = cr.id
                    AND crm.member_type = 'Relation'
       )"_M);

  // Recursively iterate over list of relation ids and extract relation member ids
  do {
    std::set<osm_nwr_id_t> next_iteration_relation_ids;

    auto r_children = m.exec_prepared("calc_child_relation_ids_for_relation_ids", calc_relation_children_ids);

    for (const auto &row : r_children) {
      auto rel_id = row["member_id"].as<osm_nwr_id_t> ();
      auto [_, inserted] = transitive_relation_children_ids.insert (rel_id);

      // Relation members are only added to next iteration if we haven't processed them before
      if (inserted)
        next_iteration_relation_ids.insert (rel_id);
    }

    calc_relation_children_ids.swap(next_iteration_relation_ids);

  } while (!calc_relation_children_ids.empty()); // stop processing if we didn't find new relations

  return transitive_relation_children_ids;
}

void
ApiDB_Relation_Updater::extend_deletion_block_to_relation_children (
    const std::set<osm_nwr_id_t> & direct_relation_ids, std::set<osm_nwr_id_t> ids_if_unused,
    std::set<osm_nwr_id_t> &relations_to_exclude_from_deletion)
{

  if (direct_relation_ids.empty() || ids_if_unused.empty())
    return;

  /*
   Assuming again that we want to delete relations 9 and 10, where relation 10 is member of
   an (external) relation 11. The called provided an "if-unsued="true".
   As we didn't throw an error message earlier on, we need to figure out all relations
   that may not be deleted.

   Following our previous example, relation_still_referenced_by_relation
   would only mark relation 10 to have an outside dependency on relation 11.

   Relation 9 ---(member of)---> Relation 10 ---(member of)---> Relation 11

   Relation 10 was already be skipped earlier on by being added
   to relations_to_exclude_from_deletion. However, we cannot not assume
   it's safe to delete relation 9, because it is still a member of relation 10.

   More generally, we need to calculate the transitive closure of all relation ids'
   children that have previously found be dependent on an external relation.

   Those relation ids need to be excluded from deletion, in case they have a relation id
   in ids_if_unused (=caller has set the "if_used" flag to true)
  */

  auto transitive_relation_children_ids = collect_recursive_relation_rel_member_ids(direct_relation_ids);

  // Mark all child relations of still referenced relations to be excluded from deletion
  for (const auto id : transitive_relation_children_ids) {
    if (ids_if_unused.contains(id)) {
      relations_to_exclude_from_deletion.insert(id);
    }
  }
}

std::vector<ApiDB_Relation_Updater::relation_t>
ApiDB_Relation_Updater::is_relation_still_referenced(
    const std::vector<relation_t> &relations) {

  /*
    Check if relation id is still referenced by "outside" relations,
    i.e. relations, that are not in the list of relations to be checked

    Assuming, we have two relations with parent/child relationships
    and we want to delete both of them. As there are no outside
    relationships, deleting is safe.

    Note: Current rails based implementation doesn't support this use case!)

    Example with dependencies to relations outside of "ids"
    ---------------------------------------------------------

    current_relations: ids:  9, 10, 11

    current_relation_members:   relation_id   -   member_id
                                   10                 9
                                   11                10

    Check if relations still referenced for rels 9 + 10 --> returns only 11

   */

  if (relations.empty())
    return relations;

  std::vector<osm_nwr_id_t> ids;
  std::set<osm_nwr_id_t>    ids_if_unused; // relation ids where if-used flag is set
  std::set<osm_nwr_id_t>    ids_without_if_unused; // relation ids without if-used flag
  std::map<osm_nwr_id_t, osm_nwr_signed_id_t> id_to_old_id;

  for (const auto &rel : relations) {
    ids.push_back(rel.id);
    if (rel.if_unused) {
      ids_if_unused.insert(rel.id);
    } else {
      ids_without_if_unused.insert(rel.id);
    }
    id_to_old_id[rel.id] = rel.old_id;
  }

  std::vector<relation_t> updated_relations = relations;
  std::set<osm_nwr_id_t> relations_to_exclude_from_deletion;

  // Determine relation ids in our list of relations ids, which appear as a relation member
  // in relations outside of our list of relations
  // (direct external dependencies for our list of relations)

  m.prepare("relation_still_referenced_by_relation",
            R"(
           WITH relations_to_check (id) AS (
               SELECT * FROM
                  UNNEST( CAST($1 AS bigint[]) )
           )
           SELECT current_relation_members.member_id,
                  string_agg(current_relations.id::text,',') AS relation_ids
           FROM current_relations
             INNER JOIN current_relation_members
                    ON current_relation_members.relation_id = current_relations.id
             INNER JOIN relations_to_check c
                    ON current_relation_members.member_id = c.id
             LEFT OUTER JOIN relations_to_check
                    ON current_relations.id = relations_to_check.id
           WHERE current_relations.visible = true
             AND current_relation_members.member_type = 'Relation'
             AND relations_to_check.id IS NULL
           GROUP BY current_relation_members.member_id
       )"_M);

  auto r = m.exec_prepared("relation_still_referenced_by_relation", ids);

  for (const auto &row : r) {
    auto rel_id = row["member_id"].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a relation that is still referenced,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.contains(rel_id)) {

      // Without the if-unused, such a situation would lead to an error, and the
      // whole diff upload would fail.
      throw http::precondition_failed(
          fmt::format("The relation {:d} is used in relations {}.",
           row["member_id"].as<osm_nwr_id_t>(),
           row["relation_ids"].c_str()));
    }

    if (ids_if_unused.contains(rel_id)) {
      /* a <delete> block in the OsmChange document may have an if-unused
       * attribute. If this attribute is present, then the delete operation(s)
       * in this block are conditional and will only be executed if the object
       * to be deleted is not used by another object.
       */

      relations_to_exclude_from_deletion.insert(
          row["member_id"].as<osm_nwr_id_t>());
    }
  }

  // Relation ids with direct dependency on external relations
  std::set<osm_nwr_id_t> direct_relation_ids;
  for (const auto &row : r) {
    auto rel_id = row["member_id"].as<osm_nwr_id_t> ();
    direct_relation_ids.insert (rel_id);
  }

  // if-used="true" requires children of relations with direct dependency
  // on external relations to be excluded from deletion
  extend_deletion_block_to_relation_children(direct_relation_ids, ids_if_unused, relations_to_exclude_from_deletion);

  // Prepare updated list of relations, which no longer contains relations that are
  // still in use by other relations. We will simply skip those relations from now on.
  remove_blocked_relations_from_deletion_list(
      relations_to_exclude_from_deletion, id_to_old_id, updated_relations);

  return updated_relations;
}

void ApiDB_Relation_Updater::delete_current_relation_members(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("delete_current_relation_members",
            "DELETE FROM current_relation_members WHERE relation_id = ANY($1)");

  auto r = m.exec_prepared("delete_current_relation_members", ids);
}

void ApiDB_Relation_Updater::delete_current_relation_tags(
    const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return;

  m.prepare("delete_current_relation_tags",
            "DELETE FROM current_relation_tags WHERE relation_id = ANY($1)");

  auto r = m.exec_prepared("delete_current_relation_tags", ids);
}

uint32_t ApiDB_Relation_Updater::get_num_changes() const {
  return (ct.created_relation_ids.size() + ct.modified_relation_ids.size() +
          ct.deleted_relation_ids.size());
}

bbox_t ApiDB_Relation_Updater::bbox() const { return m_bbox; }
