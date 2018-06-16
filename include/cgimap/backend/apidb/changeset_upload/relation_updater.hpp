#ifndef APIDB_RELATION_UPDATER
#define APIDB_RELATION_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload/relation.hpp"
#include "cgimap/api06/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#include <set>

using RelationMemberList = std::vector<RelationMember>;
using TagList = std::map<std::string, std::string>;

class ApiDB_Relation_Updater : public Relation_Updater {

public:
  ApiDB_Relation_Updater(Transaction_Manager &_m,
                         std::shared_ptr<OSMChange_Tracking> _ct);

  virtual ~ApiDB_Relation_Updater();

  void add_relation(osm_changeset_id_t changeset_id, osm_nwr_signed_id_t old_id,
                    const RelationMemberList &members, const TagList &tags);

  void modify_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                       osm_version_t version, const RelationMemberList &members,
                       const TagList &tags);

  void delete_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                       osm_version_t version, bool if_unused);

  void process_new_relations();

  void process_modify_relations();

  void process_delete_relations();

  unsigned int get_num_changes();

  bbox_t bbox();

private:
  bbox_t m_bbox;

  struct member_t {
    std::string member_type;
    osm_nwr_id_t member_id;
    std::string member_role;
    osm_sequence_id_t sequence_id;
    osm_nwr_signed_id_t old_member_id;
  };

  struct relation_t {
    osm_nwr_id_t id;
    osm_version_t version;
    osm_changeset_id_t changeset_id;
    osm_nwr_signed_id_t old_id;
    std::vector<std::pair<std::string, std::string>> tags;
    std::vector<member_t> members;
    bool if_unused;
  };

  struct rel_member_difference_t {
    std::string member_type;
    osm_nwr_id_t member_id;
    bool new_member;
  };

  void truncate_temporary_tables();

  /*
   * Set id field based on old_id -> id mapping
   *
   */
  void replace_old_ids_in_relations(
      std::vector<relation_t> &relations,
      const std::vector<OSMChange_Tracking::object_id_mapping_t>
          &created_node_id_mapping,
      const std::vector<OSMChange_Tracking::object_id_mapping_t>
          &created_way_id_mapping,
      const std::vector<OSMChange_Tracking::object_id_mapping_t>
          &created_relation_id_mapping);

  void check_unique_placeholder_ids(const std::vector<relation_t> &create_relations);

  void insert_new_relations_to_tmp_table(
      const std::vector<relation_t> &create_relations);

  void copy_tmp_create_relations_to_current_relations();

  void delete_tmp_create_relations();

  void lock_current_relations(const std::vector<osm_nwr_id_t> &ids);

  std::vector<std::vector<ApiDB_Relation_Updater::relation_t>>
  build_packages(const std::vector<relation_t> &relations);

  void
  check_current_relation_versions(const std::vector<relation_t> &relations);

  // for if-unused - determine ways to be excluded from deletion, regardless of
  // their current version
  std::set<osm_nwr_id_t>
  determine_already_deleted_relations(const std::vector<relation_t> &relations);

  void lock_future_members(const std::vector<relation_t> &relations);

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

  void
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

  Transaction_Manager &m;
  std::shared_ptr<OSMChange_Tracking> ct;

  std::vector<relation_t> create_relations;
  std::vector<relation_t> modify_relations;
  std::vector<relation_t> delete_relations;

  std::set<osm_nwr_signed_id_t> create_placedholder_ids;
};

#endif
