#include "util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/format.hpp>

using boost::format;

ApiDB_Relation_Updater::ApiDB_Relation_Updater(
    Transaction_Manager &_m, std::shared_ptr<OSMChange_Tracking> _ct)
    : m_bbox(), m(_m), ct(std::move(_ct)) {}

ApiDB_Relation_Updater::~ApiDB_Relation_Updater() {}

void ApiDB_Relation_Updater::add_relation(osm_changeset_id_t changeset_id,
                                          osm_nwr_signed_id_t old_id,
                                          const RelationMemberList &members,
                                          const TagList &tags) {

  relation_t new_relation{};

  new_relation.version = 1;
  new_relation.changeset_id = changeset_id;
  new_relation.old_id = old_id;

  for (const auto &tag : tags)
    new_relation.tags.emplace_back(
        std::pair<std::string, std::string>(tag.first, tag.second));

  int member_seq = 0;
  for (const auto &member : members) {
    member_t new_member{};
    new_member.member_type = member.type();
    new_member.member_role = member.role();
    new_member.member_id = (member.ref() < 0 ? 0 : member.ref());
    new_member.old_member_id = member.ref();
    new_member.sequence_id = member_seq++;
    new_relation.members.push_back(new_member);
  }

  create_relations.push_back(new_relation);

  ct->osmchange_orig_sequence.push_back(
      { operation::op_create, object_type::relation, new_relation.old_id,
        new_relation.version, false });
}

void ApiDB_Relation_Updater::modify_relation(osm_changeset_id_t changeset_id,
                                             osm_nwr_id_t id,
                                             osm_version_t version,
                                             const RelationMemberList &members,
                                             const TagList &tags) {

  relation_t modify_relation{};

  modify_relation.old_id = id;
  modify_relation.id = id;
  modify_relation.version = version;
  modify_relation.changeset_id = changeset_id;

  for (const auto &tag : tags)
    modify_relation.tags.emplace_back(
        std::pair<std::string, std::string>(tag.first, tag.second));

  int member_seq = 0;
  for (const auto &member : members) {
    member_t modify_member{};
    modify_member.member_type = member.type();
    modify_member.member_role = member.role();
    modify_member.member_id = (member.ref() < 0 ? 0 : member.ref());
    modify_member.old_member_id = member.ref();
    modify_member.sequence_id = member_seq++;
    modify_relation.members.push_back(modify_member);
  }

  modify_relations.push_back(modify_relation);

  ct->osmchange_orig_sequence.push_back(
      { operation::op_modify, object_type::relation, modify_relation.old_id,
        modify_relation.version, false });
}

void ApiDB_Relation_Updater::delete_relation(osm_changeset_id_t changeset_id,
                                             osm_nwr_id_t id,
                                             osm_version_t version,
                                             bool if_unused) {

  relation_t delete_relation{};
  delete_relation.old_id = id;
  delete_relation.id = id;
  delete_relation.version = version;
  delete_relation.changeset_id = changeset_id;
  delete_relation.if_unused = if_unused;
  delete_relations.push_back(delete_relation);

  ct->osmchange_orig_sequence.push_back(
      { operation::op_delete, object_type::relation, delete_relation.old_id,
        delete_relation.version, if_unused });
}

void ApiDB_Relation_Updater::process_new_relations() {

  truncate_temporary_tables();

  check_unique_placeholder_ids(create_relations);

  insert_new_relations_to_tmp_table(create_relations);
  copy_tmp_create_relations_to_current_relations();
  delete_tmp_create_relations();

  // Use new_ids as a result of inserting nodes/ways in tmp table
  replace_old_ids_in_relations(create_relations, ct->created_node_ids,
                               ct->created_way_ids, ct->created_relation_ids);

  std::vector<osm_nwr_id_t> ids;

  for (const auto &id : create_relations)
    ids.push_back(id.id);

  // remove duplicates
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  lock_current_relations(ids);
  lock_future_members(create_relations);

  insert_new_current_relation_tags(create_relations);
  insert_new_current_relation_members(create_relations);

  save_current_relations_to_history(ids);
  save_current_relation_tags_to_history(ids);
  save_current_relation_members_to_history(ids);

  m_bbox.expand(calc_relation_bbox(ids));

  create_relations.clear();
}

void ApiDB_Relation_Updater::process_modify_relations() {

  // Use new_ids as a result of inserting nodes/ways/relations in tmp table
  replace_old_ids_in_relations(modify_relations, ct->created_node_ids,
                               ct->created_way_ids, ct->created_relation_ids);

  std::vector<osm_nwr_id_t> ids;

  for (const auto &id : modify_relations)
    ids.push_back(id.id);

  // remove duplicates
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  lock_current_relations(ids);

  lock_future_members(modify_relations);

  // modify may contain several versions of the same relation
  // those have to be processed one after the other
  auto packages = build_packages(modify_relations);

  for (const auto &modify_relations_package : packages) {
    std::vector<osm_nwr_id_t> ids_package;

    for (const auto &id : modify_relations_package)
      ids_package.push_back(id.id);

    // remove duplicates
    std::sort(ids_package.begin(), ids_package.end());
    ids.erase(std::unique(ids_package.begin(), ids_package.end()),
              ids_package.end());

    check_current_relation_versions(modify_relations_package);

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
    insert_new_current_relation_tags(modify_relations_package);
    insert_new_current_relation_members(modify_relations_package);

    save_current_relations_to_history(ids_package);
    save_current_relation_tags_to_history(ids_package);
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
  replace_old_ids_in_relations(delete_relations, ct->created_node_ids,
                               ct->created_way_ids, ct->created_relation_ids);

  for (const auto &id : delete_relations)
    ids.push_back(id.id);

  // remove duplicates
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  lock_current_relations(ids);

  // In case the delete element has an "if-unused" flag, we ignore already
  // deleted relations and avoid raising an error message.

  auto already_deleted_relations =
      determine_already_deleted_relations(delete_relations);

  for (const auto &relation : delete_relations)
    if (already_deleted_relations.find(relation.id) ==
        already_deleted_relations.end()) {
      delete_relations_visible.push_back(relation);
      ids_visible.push_back(relation.id);
    }

  check_current_relation_versions(delete_relations_visible);

  auto delete_relations_visible_unreferenced =
      is_relation_still_referenced(delete_relations_visible);

  m_bbox.expand(calc_relation_bbox(ids));

  update_current_relations(delete_relations_visible_unreferenced, false);

  for (const auto &node : delete_relations_visible_unreferenced)
    ids_visible_unreferenced.push_back(node.id);

  delete_current_relation_tags(ids_visible_unreferenced);
  delete_current_relation_members(ids_visible_unreferenced);

  save_current_relations_to_history(ids_visible_unreferenced);
  // no relation members or relation tags to save here

  delete_relations.clear();
}

void ApiDB_Relation_Updater::truncate_temporary_tables() {
  m.exec("TRUNCATE TABLE tmp_create_relations");
}

/*
 * Set id field based on old_id -> id mapping
 *
 */
void ApiDB_Relation_Updater::replace_old_ids_in_relations(
    std::vector<relation_t> &relations,
    const std::vector<OSMChange_Tracking::object_id_mapping_t>
        &created_node_id_mapping,
    const std::vector<OSMChange_Tracking::object_id_mapping_t>
        &created_way_id_mapping,
    const std::vector<OSMChange_Tracking::object_id_mapping_t>
        &created_relation_id_mapping) {

  // Prepare mapping tables
  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map_relations;
  for (auto i : created_relation_id_mapping) {
    auto res = map_relations.insert(
        std::pair<osm_nwr_signed_id_t, osm_nwr_id_t>(i.old_id, i.new_id));
    if (!res.second)
      throw http::bad_request(
          (boost::format("Duplicate relation placeholder id %1%.") % i.old_id)
              .str());
  }

  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map_ways;
  for (auto i : created_way_id_mapping) {
    auto res = map_ways.insert(
        std::pair<osm_nwr_signed_id_t, osm_nwr_id_t>(i.old_id, i.new_id));
    if (!res.second)
      throw http::bad_request(
          (boost::format("Duplicate way placeholder id %1%.") % i.old_id)
              .str());
  }

  std::map<osm_nwr_signed_id_t, osm_nwr_id_t> map_nodes;
  for (auto i : created_node_id_mapping) {
    auto res = map_nodes.insert(
        std::pair<osm_nwr_signed_id_t, osm_nwr_id_t>(i.old_id, i.new_id));
    if (!res.second)
      throw http::bad_request(
          (boost::format("Duplicate node placeholder id %1%.") % i.old_id)
              .str());
  }

  // replace temporary ids
  for (auto &cr : relations) {
    // TODO: Check if this is possible in case of replace
    if (cr.old_id < 0) {
      auto entry = map_relations.find(cr.old_id);
      if (entry == map_relations.end())
        throw http::bad_request(
            (boost::format(
                 "Placeholder id not found for relation reference %1%") %
             cr.old_id)
                .str());

      cr.id = entry->second;
    }

    for (auto &mbr : cr.members) {
      if (mbr.old_member_id < 0) {
        if (mbr.member_type == "Node") {
          auto entry = map_nodes.find(mbr.old_member_id);
          if (entry == map_nodes.end())
            throw http::bad_request(
                (boost::format("Placeholder node not found for reference %1% "
                               "in relation %2%") %
                 mbr.old_member_id % cr.old_id)
                    .str());
          mbr.member_id = entry->second;
        } else if (mbr.member_type == "Way") {
          auto entry = map_ways.find(mbr.old_member_id);
          if (entry == map_ways.end())
            throw http::bad_request(
                (boost::format("Placeholder way not found for reference %1% in "
                               "relation %2%") %
                 mbr.old_member_id % cr.old_id)
                    .str());
          mbr.member_id = entry->second;

        } else if (mbr.member_type == "Relation") {
          auto entry = map_relations.find(mbr.old_member_id);
          if (entry == map_relations.end())
            throw http::bad_request(
                (boost::format("Placeholder relation not found for reference "
                               "%1% in relation %2%") %
                 mbr.old_member_id % cr.old_id)
                    .str());
          mbr.member_id = entry->second;
        }
      }
    }
  }
}

void ApiDB_Relation_Updater::check_unique_placeholder_ids(
    const std::vector<relation_t> &create_relations) {

  for (const auto &create_relation : create_relations) {
    auto res = create_placedholder_ids.insert(create_relation.old_id);

    if (!res.second)
      throw http::bad_request(
          "Placeholder IDs must be unique for created elements.");
  }
}

void ApiDB_Relation_Updater::insert_new_relations_to_tmp_table(
    const std::vector<relation_t> &create_relations) {

  m.prepare("insert_tmp_create_relations", R"(
        
        WITH tmp_rel (changeset_id, old_id) AS (
           SELECT * FROM
           UNNEST( CAST($1 AS bigint[]),
                   CAST($2 AS bigint[])
                 )
        )
        INSERT INTO tmp_create_relations (changeset_id, old_id)
        SELECT * FROM tmp_rel
        RETURNING id, old_id
    )");

  std::vector<osm_changeset_id_t> cs;
  std::vector<osm_nwr_signed_id_t> oldids;

  for (const auto &create_relation : create_relations) {
    cs.emplace_back(create_relation.changeset_id);
    oldids.emplace_back(create_relation.old_id);
  }

  pqxx::result r = m.prepared("insert_tmp_create_relations")(cs)(oldids).exec();

  if (r.affected_rows() != create_relations.size())
    throw http::server_error(
        "Could not create all new relations in temporary table");

  for (const auto &row : r)
    ct->created_relation_ids.push_back(
        { row["old_id"].as<osm_nwr_signed_id_t>(), row["id"].as<osm_nwr_id_t>(),
          1 });
}

void ApiDB_Relation_Updater::copy_tmp_create_relations_to_current_relations() {

  m.exec(
      R"(
        INSERT INTO current_relations 
               (SELECT id, changeset_id, timestamp, visible, version from tmp_create_relations)
        )");
}

void ApiDB_Relation_Updater::delete_tmp_create_relations() {

  m.exec("DELETE FROM tmp_create_relations");
}

void ApiDB_Relation_Updater::lock_current_relations(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("lock_current_relations",
            "SELECT id FROM current_relations WHERE id = ANY($1) FOR UPDATE");

  pqxx::result r = m.prepared("lock_current_relations")(ids).exec();

  std::set<osm_nwr_id_t> locked_ids;

  for (const auto &row : r)
    locked_ids.insert(row["id"].as<osm_nwr_id_t>());

  std::set<osm_nwr_id_t> idset(ids.begin(), ids.end());

  if (idset.size() != locked_ids.size()) {
    std::set<osm_nwr_id_t> not_locked_ids;

    std::set_difference(idset.begin(), idset.end(), locked_ids.begin(),
                        locked_ids.end(),
                        std::inserter(not_locked_ids, not_locked_ids.begin()));

    throw http::not_found(
        (boost::format("The following relation ids are unknown: %1%") %
         to_string(not_locked_ids))
            .str());
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
    if (id_to_package.find(relation.id) == id_to_package.end())
      id_to_package[relation.id] = 0;
    else
      ++id_to_package[relation.id];

    if (id_to_package[relation.id] + 1 > result.size())
      result.emplace_back(std::vector<ApiDB_Relation_Updater::relation_t>());

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
                               CAST($2 as bigint[])
                             )
                  )
                  SELECT t.id, 
                         t.version                 AS expected_version, 
                         current_relations.version AS actual_version
                  FROM tmp_relation_versions t
                  INNER JOIN current_relations
                     ON t.id = current_relations.id
                  WHERE t.version <> current_relations.version
                  LIMIT 1
       )");

  pqxx::result r =
      m.prepared("check_current_relation_versions")(ids)(versions).exec();

  if (!r.empty()) {
    throw http::conflict((boost::format("Version mismatch: Provided %1%, "
                                        "server had: %2% of Relation %3%") %
                          r[0]["expected_version"].as<long>() %
                          r[0]["actual_version"].as<long>() %
                          r[0]["id"].as<long>())
                             .str());
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

  for (const auto &relation : relations) {
    ids_to_be_deleted.insert(relation.id);
    if (relation.if_unused) {
      ids_if_unused.insert(relation.id);
    } else {
      ids_without_if_unused.insert(relation.id);
    }
  }

  if (ids_to_be_deleted.empty())
    return result;

  m.prepare("already_deleted_relations", "SELECT id, version FROM "
                                         "current_relations WHERE id = ANY($1) "
                                         "AND visible = false");

  pqxx::result r =
      m.prepared("already_deleted_relations")(ids_to_be_deleted).exec();

  for (const auto &row : r) {
    osm_nwr_id_t id = row["id"].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a relation that is already deleted,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.find(id) != ids_without_if_unused.end()) {
      throw http::gone(
          (boost::format(
               "The relation with the id %1% has already been deleted") %
           id).str());
    }

    result.insert(id);

    // if-used flag is set:
    // We have identified a relation that is already deleted on the server. The
    // only thing left to do in this scenario is to return old_id, new_id and
    // the current version to the caller

    if (ids_if_unused.find(id) != ids_if_unused.end()) {

      ct->skip_deleted_relation_ids.push_back(
          { row["id"].as<long>(), row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>() });
    }
  }

  return result;
}

void ApiDB_Relation_Updater::lock_future_members(
    const std::vector<relation_t> &relations) {

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
      else if (rm.member_type == "Relation")
        relation_ids.push_back(rm.member_id);
    }
  }

  if (node_ids.empty() && way_ids.empty() && relation_ids.empty())
    return; // nothing to do

  // remove duplicates
  std::sort(node_ids.begin(), node_ids.end());
  node_ids.erase(std::unique(node_ids.begin(), node_ids.end()), node_ids.end());

  std::sort(way_ids.begin(), way_ids.end());
  way_ids.erase(std::unique(way_ids.begin(), way_ids.end()), way_ids.end());

  std::sort(relation_ids.begin(), relation_ids.end());
  relation_ids.erase(std::unique(relation_ids.begin(), relation_ids.end()),
                     relation_ids.end());

  // TODO: check if we should exclude our own list of relation ids from the
  // check

  // sequence nodes/way/relations??

  if (!node_ids.empty()) {
    m.prepare("lock_future_nodes_in_relations",
              R"( 
                  SELECT id
                  FROM current_nodes
                  WHERE visible = true 
                  AND id = ANY($1) FOR SHARE 
              )");

    pqxx::result r =
        m.prepared("lock_future_nodes_in_relations")(node_ids).exec();

    if (r.size() != node_ids.size()) {
      std::set<osm_nwr_id_t> locked_nodes;

      for (const auto &row : r)
        locked_nodes.insert(row["id"].as<osm_nwr_id_t>());

      std::map<osm_nwr_signed_id_t, std::set<osm_nwr_id_t>> absent_rel_node_ids;

      for (const auto &rel : relations)
        for (const auto &rm : rel.members)
          if (rm.member_type == "Node" &&
              locked_nodes.find(rm.member_id) == locked_nodes.end())
            absent_rel_node_ids[rel.old_id].insert(
                rm.member_id); // return rel id in osmChange for error msg

      auto it = absent_rel_node_ids.begin();

      throw http::precondition_failed(
          (boost::format("Relation %1% requires the nodes with id in %2%, "
                         "which either do not exist, or are not visible.") %
           it->first % to_string(it->second))
              .str());
    }
  }

  if (!way_ids.empty()) {
    m.prepare("lock_future_ways_in_relations",
              R"( 
                SELECT id
                FROM current_ways
                WHERE visible = true 
                AND id = ANY($1) FOR SHARE 
             )");

    pqxx::result r =
        m.prepared("lock_future_ways_in_relations")(way_ids).exec();

    if (r.size() != way_ids.size()) {
      std::set<osm_nwr_id_t> locked_ways;

      for (const auto &row : r)
        locked_ways.insert(row["id"].as<osm_nwr_id_t>());

      std::map<osm_nwr_signed_id_t, std::set<osm_nwr_id_t>> absent_rel_way_ids;

      for (const auto &rel : relations)
        for (const auto &rm : rel.members)
          if (rm.member_type == "Way" &&
              locked_ways.find(rm.member_id) == locked_ways.end())
            absent_rel_way_ids[rel.old_id].insert(
                rm.member_id); // return rel id in osmChange for error msg

      auto it = absent_rel_way_ids.begin();

      throw http::precondition_failed(
          (boost::format("Relation %1% requires the ways with id in %2%, which "
                         "either do not exist, or are not visible.") %
           it->first % to_string(it->second))
              .str());
    }
  }

  if (!relation_ids.empty()) {
    m.prepare("lock_future_relations_in_relations",
              R"( 
                SELECT id
                FROM current_relations
                WHERE visible = true 
                AND id = ANY($1) FOR SHARE )");

    pqxx::result r =
        m.prepared("lock_future_relations_in_relations")(relation_ids).exec();

    if (r.size() != relation_ids.size()) {
      std::set<osm_nwr_id_t> locked_relations;

      for (const auto &row : r)
        locked_relations.insert(row["id"].as<osm_nwr_id_t>());

      std::map<osm_nwr_signed_id_t, std::set<osm_nwr_id_t>> absent_rel_rel_ids;

      for (const auto &rel : relations)
        for (const auto &rm : rel.members)
          if (rm.member_type == "Relation" &&
              locked_relations.find(rm.member_id) == locked_relations.end())
            absent_rel_rel_ids[rel.old_id].insert(
                rm.member_id); // return rel id in osmChange for error msg

      auto it = absent_rel_rel_ids.begin();

      throw http::precondition_failed(
          (boost::format("Relation %1% requires the relations with id in %2%, "
                         "which either do not exist, or are not visible.") %
           it->first % to_string(it->second))
              .str());
    }
  }
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
     )");

  pqxx::result r = m.prepared("relations_with_new_relation_members")(
                        relation_ids)(member_ids)
                       .exec();

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
    for (const auto &tag : relation.tags) {
      ids.push_back(relation.id);
      ks.push_back(escape(tag.first));
      vs.push_back(escape(tag.second));
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
         )");

  pqxx::result r =
      m.prepared("relations_with_changed_relation_tags")(ids)(ks)(vs).exec();

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

  m.prepare("relations_with_changed_way_node_members",
            R"(  
            WITH tmp_member(relation_id, member_type, member_id) AS (
                 SELECT * FROM
                 UNNEST( CAST($1 as bigint[]),
                         CAST($2 as nwr_enum[]),
                         CAST($3 as bigint[])
                 )
              )
            /* new member was added in tmp */
            SELECT tm.member_type, tm.member_id, true as new_member
                   FROM   tmp_member tm
                   LEFT OUTER JOIN current_relation_members cm
                     ON tm.relation_id = cm.relation_id
                    AND tm.member_type = cm.member_type
                    AND tm.member_id   = cm.member_id
                 WHERE cm.member_id IS NULL
            UNION 
            /* existing member was removed in tmp */
            SELECT cm.member_type, cm.member_id, false as new_member
               FROM current_relation_members cm
               INNER JOIN tmp_member t1
                  ON cm.relation_id = t1.relation_id
               LEFT OUTER JOIN tmp_member tm
                  ON cm.relation_id = tm.relation_id
                 AND cm.member_type = tm.member_type
                 AND cm.member_id   = tm.member_id
            WHERE tm.member_id IS NULL

         )");

  pqxx::result r = m.prepared("relations_with_changed_way_node_members")(ids)(
                        membertypes)(memberids)
                       .exec();

  for (const auto &row : r) {
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
       )");

    pqxx::result r = m.prepared("calc_node_bbox_rel_member")(node_ids).exec();

    if (!(r.empty() || r[0]["minlat"].is_null())) {
      bbox_nodes.minlat = r[0]["minlat"].as<long>();
      bbox_nodes.minlon = r[0]["minlon"].as<long>();
      bbox_nodes.maxlat = r[0]["maxlat"].as<long>();
      bbox_nodes.maxlon = r[0]["maxlon"].as<long>();
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
       )");

    pqxx::result r = m.prepared("calc_way_bbox_rel_member")(way_ids).exec();

    if (!(r.empty() || r[0]["minlat"].is_null())) {
      bbox_ways.minlat = r[0]["minlat"].as<long>();
      bbox_ways.minlon = r[0]["minlon"].as<long>();
      bbox_ways.maxlat = r[0]["maxlat"].as<long>();
      bbox_ways.maxlon = r[0]["maxlon"].as<long>();
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
                FROM current_nodes
                INNER JOIN current_relation_members
                        ON current_relation_members.member_id = current_nodes.id
                INNER JOIN current_relations
                        ON current_relations.id = current_relation_members.relation_id
                 WHERE current_relations.visible = true
                   AND current_relation_members.member_type = 'Node'
                   AND current_relations.id = ANY($1)
            )");

  pqxx::result rn = m.prepared("calc_relation_bbox_nodes")(ids).exec();

  if (!(rn.empty() || rn[0]["minlat"].is_null())) {
    bbox.minlat = rn[0]["minlat"].as<long>();
    bbox.minlon = rn[0]["minlon"].as<long>();
    bbox.maxlat = rn[0]["maxlat"].as<long>();
    bbox.maxlon = rn[0]["maxlon"].as<long>();
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
                INNER JOIN current_relation_members
                        ON current_relation_members.member_id = w.id
                INNER JOIN current_relations
                        ON current_relations.id = current_relation_members.relation_id
                 WHERE current_relations.visible = true
                   AND current_relation_members.member_type = 'Way'
                   AND current_relations.id = ANY($1)
              )");

  pqxx::result rw = m.prepared("calc_relation_bbox_ways")(ids).exec();

  if (!(rw.empty() || rw[0]["minlat"].is_null())) {
    bbox_t bbox_way;

    bbox_way.minlat = rw[0]["minlat"].as<long>();
    bbox_way.minlon = rw[0]["minlon"].as<long>();
    bbox_way.maxlat = rw[0]["maxlat"].as<long>();
    bbox_way.maxlon = rw[0]["maxlon"].as<long>();
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
        WITH u(id, changeset_id, visible, version) AS (
                SELECT * FROM
                UNNEST( CAST($1 AS bigint[]),
                        CAST($2 AS bigint[]),
                        CAST($3 AS boolean[]),
                        CAST($4 AS bigint[])
                      )
        )
        UPDATE current_relations AS r
        SET changeset_id = u.changeset_id,
            visible = u.visible,
            timestamp = (now() at time zone 'utc'),
            version = u.version + 1
        FROM u
        WHERE r.id = u.id
          AND r.version = u.version
        RETURNING r.id, r.version 
    )");

  std::vector<osm_nwr_signed_id_t> ids;
  std::vector<osm_changeset_id_t> cs;
  std::vector<bool> visibles;
  std::vector<osm_version_t> versions;

  for (const auto &relation : relations) {
    ids.emplace_back(relation.id);
    cs.emplace_back(relation.changeset_id);
    visibles.push_back(visible);
    versions.emplace_back(relation.version);
  }

  pqxx::result r =
      m.prepared("update_current_relations")(ids)(cs)(visibles)(versions)
          .exec();

  if (r.affected_rows() != relations.size())
    throw http::server_error("Could not update all current relations");

  // update modified/deleted relations table
  for (const auto &row : r) {
    if (visible) {
      ct->modified_relation_ids.push_back(
          { row["id"].as<long>(), row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>() });
    } else {
      ct->deleted_relation_ids.push_back({ row["id"].as<osm_nwr_id_t>() });
    }
  }
}

void ApiDB_Relation_Updater::insert_new_current_relation_tags(
    const std::vector<relation_t> &relations) {

  if (relations.empty())
    return;

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
            )");

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  for (const auto &relation : relations)
    for (const auto &tag : relation.tags) {
      ids.emplace_back(relation.id);
      ks.emplace_back(escape(tag.first));
      vs.emplace_back(escape(tag.second));
    }

  pqxx::result r =
      m.prepared("insert_new_current_relation_tags")(ids)(ks)(vs).exec();
}

void ApiDB_Relation_Updater::insert_new_current_relation_members(
    const std::vector<relation_t> &relations) {

  if (relations.empty())
    return;

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
      )");

  std::vector<osm_nwr_id_t> ids;
  std::vector<std::string> membertypes;
  std::vector<osm_nwr_id_t> memberids;
  std::vector<std::string> memberroles;
  std::vector<long> sequenceids;

  for (const auto &relation : relations)
    for (const auto &member : relation.members) {
      ids.emplace_back(relation.id);
      membertypes.emplace_back(member.member_type);
      memberids.emplace_back(member.member_id);
      memberroles.emplace_back(escape(member.member_role));
      sequenceids.emplace_back(member.sequence_id);
    }

  pqxx::result r = m.prepared("insert_new_current_relation_members")(ids)(
                        membertypes)(memberids)(memberroles)(sequenceids)
                       .exec();
}

void ApiDB_Relation_Updater::save_current_relations_to_history(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("current_relations_to_history",
            R"(   
                INSERT INTO relations 
                (SELECT id AS relation_id, changeset_id, timestamp, version, visible
                 FROM current_relations
                 WHERE id = ANY($1)) 
            )");

  pqxx::result r = m.prepared("current_relations_to_history")(ids).exec();

  if (r.affected_rows() != ids.size())
    throw http::server_error("Could not save current relations to history");
}

void ApiDB_Relation_Updater::save_current_relation_tags_to_history(
    const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return;

  m.prepare("current_relation_tags_to_history",
            R"(   
                INSERT INTO relation_tags ( 
                 SELECT relation_id, k, v, version FROM current_relation_tags rt
                 INNER JOIN current_relations r
                 ON rt.relation_id = r.id 
                 WHERE id = ANY($1)) 
             )");

  pqxx::result r = m.prepared("current_relation_tags_to_history")(ids).exec();
}

void ApiDB_Relation_Updater::save_current_relation_members_to_history(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("current_relation_members_to_history",
            R"(   
                INSERT INTO relation_members ( 
                 SELECT relation_id, member_type, member_id, member_role,
                        version, sequence_id 
                 FROM current_relation_members rm
                 INNER JOIN current_relations r
                 ON rm.relation_id = r.id
                 WHERE id = ANY($1))
                          )");

  pqxx::result r =
      m.prepared("current_relation_members_to_history")(ids).exec();
}

std::vector<ApiDB_Relation_Updater::relation_t>
ApiDB_Relation_Updater::is_relation_still_referenced(
    const std::vector<relation_t> &relations) {

  /*
   * Check if relation id is still referenced by "outside" relations,
   * i.e. relations, that are not in the list of relations to be checked
   *
   * Assuming, we have two relations with parent/child relationships
   * and we want to delete both of them. As there are no outside
   * relationships, deleting is safe.
   *
   * Note: Current rails based implementation doesn't support this use case!)
   *
   * Example with dependencies to relations outside of "ids"
   * ---------------------------------------------------------
   *
   * current_relations: ids:  9, 10, 11
   *
   * current_relation_members:   relation_id   -   member_id
   *                                10                 9
   *                                11                10
   *
   * Check if relations still referenced for rels 9 + 10 --> returns only 11
   *
   */

  if (relations.empty())
    return relations;

  std::vector<osm_nwr_id_t> ids;
  std::set<osm_nwr_id_t>
      ids_if_unused; // relation ids where if-used flag is set
  std::set<osm_nwr_id_t>
      ids_without_if_unused; // relation ids without if-used flag

  for (const auto &id : relations) {
    ids.push_back(id.id);
    if (id.if_unused) {
      ids_if_unused.insert(id.id);
    } else {
      ids_without_if_unused.insert(id.id);
    }
  }

  std::vector<relation_t> updated_relations = relations;
  std::set<osm_nwr_id_t> relations_to_exclude_from_deletion;

  m.prepare("relation_still_referenced_by_relation",
            R"(
           WITH relations_to_check (id) AS (
               SELECT * FROM
                  UNNEST( CAST($1 AS bigint[]) )
           )
           SELECT current_relation_members.member_id, 
                  array_agg(current_relations.id) AS relation_ids 
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
       )");

  pqxx::result r =
      m.prepared("relation_still_referenced_by_relation")(ids).exec();

  for (const auto &row : r) {
    auto rel_id = row["member_id"].as<osm_nwr_id_t>();

    // OsmChange documents wants to delete a relation that is still referenced,
    // and the if-unused flag hasn't been set!
    if (ids_without_if_unused.find(rel_id) != ids_without_if_unused.end()) {

      // Without the if-unused, such a situation would lead to an error, and the
      // whole diff upload would fail.
      throw http::precondition_failed(
          (boost::format("The relation %1% is used in relations %2%.") %
           row["member_id"].as<osm_nwr_id_t>() %
           friendly_name(row["relation_ids"].as<std::string>()))
              .str());
    }

    if (ids_if_unused.find(rel_id) != ids_if_unused.end()) {
      /* a <delete> block in the OsmChange document may have an if-unused
       * attribute. If this attribute is present, then the delete operation(s)
       * in this block are conditional and will only be executed if the object
       * to be deleted is not used by another object.
       */

      relations_to_exclude_from_deletion.insert(
          row["member_id"].as<osm_nwr_id_t>());
    }
  }

  // Prepare updated list of ways, which no longer contains object that are
  // still in use by relations
  // We will simply skip those nodes from now on

  if (!relations_to_exclude_from_deletion.empty()) {
    updated_relations.erase(
        std::remove_if(updated_relations.begin(), updated_relations.end(),
                       [&](const relation_t &a) {
                         return relations_to_exclude_from_deletion.find(a.id) !=
                                relations_to_exclude_from_deletion.end();
                       }),
        updated_relations.end());

    // Return old_id, new_id and current version to the caller in case of
    // if-unused, so it's clear that the delete operation was *not* executed,
    // but simply skipped

    m.prepare("still_referenced_relations",
              "SELECT id, version FROM current_relations WHERE id = ANY($1)");

    pqxx::result r = m.prepared("still_referenced_relations")(
                          relations_to_exclude_from_deletion)
                         .exec();

    if (r.affected_rows() != relations_to_exclude_from_deletion.size())
      throw http::server_error(
          "Could not get details about still referenced relations");

    std::set<osm_nwr_id_t> result;

    for (const auto &row : r) {
      result.insert(row["id"].as<osm_nwr_id_t>());

      // We have identified a node that is still used in a way or relation.
      // However, the caller has indicated via if-unused flag that deletion
      // should not lead to an error. All we can do now is to return old_id,
      // new_id and the current version to the caller

      ct->skip_deleted_relation_ids.push_back(
          { row["id"].as<long>(), row["id"].as<osm_nwr_id_t>(),
            row["version"].as<osm_version_t>() });
    }
  }

  return updated_relations;
}

void ApiDB_Relation_Updater::delete_current_relation_members(
    const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return;

  m.prepare("delete_current_relation_members",
            "DELETE FROM current_relation_members WHERE relation_id = ANY($1)");

  pqxx::result r = m.prepared("delete_current_relation_members")(ids).exec();
}

void ApiDB_Relation_Updater::delete_current_relation_tags(
    const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return;

  m.prepare("delete_current_relation_tags",
            "DELETE FROM current_relation_tags WHERE relation_id = ANY($1)");

  pqxx::result r = m.prepared("delete_current_relation_tags")(ids).exec();
}

unsigned int ApiDB_Relation_Updater::get_num_changes() {
  return (ct->created_relation_ids.size() + ct->modified_relation_ids.size() +
          ct->deleted_relation_ids.size());
}

bbox_t ApiDB_Relation_Updater::bbox() { return m_bbox; }
