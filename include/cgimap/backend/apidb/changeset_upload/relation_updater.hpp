/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef APIDB_RELATION_UPDATER
#define APIDB_RELATION_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload/relation.hpp"
#include "cgimap/api06/changeset_upload/relation_updater.hpp"

#include <set>
#include <map>
#include <string>

struct RequestContext;
class Transaction_Manager;

using RelationMemberList = std::vector<api06::RelationMember>;
using TagList = std::map<std::string, std::string>;

class ApiDB_Relation_Updater : public api06::Relation_Updater {

public:
  ApiDB_Relation_Updater(Transaction_Manager &_m,
                         const RequestContext& _req_ctx,
                         api06::OSMChange_Tracking &ct);

  ~ApiDB_Relation_Updater() override = default;

  void add_relation(osm_changeset_id_t changeset_id, osm_nwr_signed_id_t old_id,
                    const RelationMemberList &members, const TagList &tags) override;

  void modify_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                       osm_version_t version, const RelationMemberList &members,
                       const TagList &tags) override;

  void delete_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                       osm_version_t version, bool if_unused) override;

  void process_new_relations() override;

  void process_modify_relations() override;

  void process_delete_relations() override;

  unsigned int get_num_changes() const override;

  bbox_t bbox() const override;

private:
  bbox_t m_bbox{};

  struct member_t {
    std::string member_type{};
    osm_nwr_id_t member_id{};
    std::string member_role{};
    osm_sequence_id_t sequence_id{};
    osm_nwr_signed_id_t old_member_id{};
  };

  struct relation_t {
    osm_nwr_id_t id{};
    osm_version_t version{};
    osm_changeset_id_t changeset_id{};
    osm_nwr_signed_id_t old_id{};
    std::vector<std::pair<std::string, std::string>> tags{};
    std::vector<member_t> members{};
    bool if_unused{};
  };

  struct rel_member_difference_t {
    std::string member_type;
    osm_nwr_id_t member_id;
    bool new_member;
  };

  /*
   * Set id field based on old_id -> id mapping
   *
   */
  void replace_old_ids_in_relations(
      std::vector<relation_t> &relations,
      const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
          &created_node_id_mapping,
      const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
          &created_way_id_mapping,
      const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
          &created_relation_id_mapping);

  void check_unique_placeholder_ids(const std::vector<relation_t> &create_relations);

  void check_forward_relation_placeholders(const std::vector<relation_t> &create_relations);

  void insert_new_relations_to_current_table(
      const std::vector<relation_t> &create_relations);

  void lock_current_relations(const std::vector<osm_nwr_id_t> &ids);

  std::vector<std::vector<ApiDB_Relation_Updater::relation_t>>
  build_packages(const std::vector<relation_t> &relations);

  void
  check_current_relation_versions(const std::vector<relation_t> &relations);

  // for if-unused - determine ways to be excluded from deletion, regardless of
  // their current version
  std::set<osm_nwr_id_t>
  determine_already_deleted_relations(const std::vector<relation_t> &relations);

  void lock_future_members_nodes(std::vector< osm_nwr_id_t >& node_ids,
    const std::vector< relation_t > &relations);

  void lock_future_members_ways(std::vector< osm_nwr_id_t >& way_ids,
    const std::vector< relation_t > &relations);

  void lock_future_members_relations(std::vector< osm_nwr_id_t >& relation_ids,
    const std::vector< relation_t > &relations);

  void lock_future_members(const std::vector<relation_t> &relations,
			   const std::vector<osm_nwr_id_t>& already_locked_relations);

  std::set<osm_nwr_id_t>
  relations_with_new_relation_members(const std::vector<relation_t> &relations);

  std::set<osm_nwr_id_t> relations_with_changed_relation_tags(
      const std::vector<relation_t> &relations);

  std::vector<ApiDB_Relation_Updater::rel_member_difference_t>
  relations_with_changed_way_node_members(
      const std::vector<relation_t> &relations);

  bbox_t calc_rel_member_difference_bbox(
      std::vector<ApiDB_Relation_Updater::rel_member_difference_t> &diff,
      bool process_new_elements);

  bbox_t calc_relation_bbox(const std::vector<osm_nwr_id_t> &ids);

  void update_current_relations(const std::vector<relation_t> &relations,
                                bool visible);

  [[nodiscard]] std::vector<osm_nwr_id_t>
  insert_new_current_relation_tags(const std::vector<relation_t> &relations);

  void
  insert_new_current_relation_members(const std::vector<relation_t> &relations);

  void save_current_relations_to_history(const std::vector<osm_nwr_id_t> &ids);

  void
  save_current_relation_tags_to_history(const std::vector<osm_nwr_id_t> &ids);

  void save_current_relation_members_to_history(
      const std::vector<osm_nwr_id_t> &ids);

  std::vector<ApiDB_Relation_Updater::relation_t>
  is_relation_still_referenced(const std::vector<relation_t> &relations);

  void delete_current_relation_members(const std::vector<osm_nwr_id_t> &ids);

  void delete_current_relation_tags(const std::vector<osm_nwr_id_t> &ids);
  void
  remove_blocked_relations_from_deletion_list (
      std::set<osm_nwr_id_t> relations_to_exclude_from_deletion,
      std::map<osm_nwr_id_t, osm_nwr_signed_id_t> &id_to_old_id,
      std::vector<relation_t> &updated_relations);

  void
  extend_deletion_block_to_relation_children (
      const std::set<osm_nwr_id_t> & direct_relation_ids, std::set<osm_nwr_id_t> ids_if_unused,
      std::set<osm_nwr_id_t> &relations_to_exclude_from_deletion);
  std::set<osm_nwr_id_t>
  collect_recursive_relation_rel_member_ids (
      const std::set<osm_nwr_id_t> &direct_relation_ids);

  Transaction_Manager &m;
  const RequestContext& req_ctx;
  api06::OSMChange_Tracking &ct;

  std::vector<relation_t> create_relations;
  std::vector<relation_t> modify_relations;
  std::vector<relation_t> delete_relations;

  std::set<osm_nwr_signed_id_t> create_placeholder_ids;
};

#endif
