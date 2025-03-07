/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef APIDB_WAY_UPDATER
#define APIDB_WAY_UPDATER

#include "cgimap/types.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload/way_updater.hpp"
#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <set>
#include <vector>

struct RequestContext;
class Transaction_Manager;

/*  Way operations
 *
 */

class ApiDB_Way_Updater : public api06::Way_Updater {

public:
  ApiDB_Way_Updater(Transaction_Manager &_m,
                    const RequestContext& _req_ctx,
                    api06::OSMChange_Tracking &ct);

  ~ApiDB_Way_Updater() override = default;

  void add_way(osm_changeset_id_t changeset_id, osm_nwr_signed_id_t old_id,
               const api06::WayNodeList &nodes, const api06::TagList &tags) override;

  void modify_way(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                  osm_version_t version, const api06::WayNodeList &nodes,
                  const api06::TagList &tags) override;

  void delete_way(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                  osm_version_t version, bool if_unused) override;

  void process_new_ways() override;

  void process_modify_ways() override;

  void process_delete_ways() override;

  unsigned int get_num_changes() const override;

  bbox_t bbox() const override;

private:
  bbox_t m_bbox{};

  struct way_node_t {
    osm_nwr_id_t node_id;
    osm_sequence_id_t sequence_id;
    osm_nwr_signed_id_t old_node_id;
  };

  struct way_t {
    osm_nwr_id_t id{};
    osm_version_t version{};
    osm_changeset_id_t changeset_id{};
    osm_nwr_signed_id_t old_id{};
    std::vector<std::pair<std::string, std::string> > tags{};
    std::vector<way_node_t> way_nodes{};
    bool if_unused{};
  };

  /*
   * Set id field based on old_id -> id mapping
   *
   */

  void replace_old_ids_in_ways(
      std::vector<way_t> &ways,
      const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
          &created_node_id_mapping,
      const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
          &created_way_id_mapping);

  void check_unique_placeholder_ids(const std::vector<way_t> &create_ways);

  void insert_new_ways_to_current_table(const std::vector<way_t> &create_ways);

  bbox_t calc_way_bbox(const std::vector<osm_nwr_id_t> &ids);

  void lock_current_ways(const std::vector<osm_nwr_id_t> &ids);

  std::vector<std::vector<ApiDB_Way_Updater::way_t> >
  build_packages(const std::vector<way_t> &ways);

  void check_current_way_versions(const std::vector<way_t> &ways);

  // for if-unused - determine ways to be excluded from deletion, regardless of
  // their current version
  std::set<osm_nwr_id_t>
  determine_already_deleted_ways(const std::vector<way_t> &ways);

  void lock_future_nodes(const std::vector<way_t> &ways);

  void update_current_ways(const std::vector<way_t> &ways, bool visible);

  [[nodiscard]] std::vector<osm_nwr_id_t> insert_new_current_way_tags(const std::vector<way_t> &ways);

  void insert_new_current_way_nodes(const std::vector<way_t> &ways);

  void save_current_ways_to_history(const std::vector<osm_nwr_id_t> &ids);

  void save_current_way_nodes_to_history(const std::vector<osm_nwr_id_t> &ids);

  void save_current_way_tags_to_history(const std::vector<osm_nwr_id_t> &ids);

  std::vector<ApiDB_Way_Updater::way_t>
  is_way_still_referenced(const std::vector<way_t> &ways);

  void delete_current_way_tags(const std::vector<osm_nwr_id_t> &ids);

  void delete_current_way_nodes(const std::vector<osm_nwr_id_t> &ids);

  Transaction_Manager &m;
  const RequestContext& req_ctx;
  api06::OSMChange_Tracking &ct;

  std::vector<way_t> create_ways;
  std::vector<way_t> modify_ways;
  std::vector<way_t> delete_ways;

  std::set<osm_nwr_signed_id_t> create_placeholder_ids;
};

#endif /* APIDB_WAY_UPDATER */
