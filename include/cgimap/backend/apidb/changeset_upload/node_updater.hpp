/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef APIDB_NODE_UPDATER
#define APIDB_NODE_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

#include <set>
#include <vector>

struct RequestContext;
class Transaction_Manager;


class ApiDB_Node_Updater : public api06::Node_Updater {

public:
  ApiDB_Node_Updater(Transaction_Manager &_m,
                     const RequestContext& req_ctx,
                     api06::OSMChange_Tracking &ct);

  ~ApiDB_Node_Updater() override = default;

  void add_node(double lat, double lon, osm_changeset_id_t changeset_id,
                osm_nwr_signed_id_t old_id, const api06::TagList &tags) override;

  void modify_node(double lat, double lon, osm_changeset_id_t changeset_id,
                   osm_nwr_id_t id, osm_version_t version, const api06::TagList &tags) override;

  void delete_node(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                   osm_version_t version, bool if_unused) override;

  void process_new_nodes() override;

  void process_modify_nodes() override;

  void process_delete_nodes() override;

  unsigned int get_num_changes() const override;

  bbox_t bbox() const override;

private:
  bbox_t m_bbox{};

  struct node_t {
    osm_nwr_id_t id{};
    osm_version_t version{};
    int64_t lat{};
    int64_t lon{};
    uint64_t tile{};
    osm_changeset_id_t changeset_id{};
    osm_nwr_signed_id_t old_id{};
    std::vector<std::pair<std::string, std::string> > tags{};
    bool if_unused{};
  };

  void replace_old_ids_in_nodes(
      std::vector<node_t> &create_nodes,
      const std::vector<api06::OSMChange_Tracking::object_id_mapping_t>
          &created_node_id_mapping);

  void check_unique_placeholder_ids(const std::vector<node_t> &create_nodes);

  void insert_new_nodes_to_current_table(const std::vector<node_t> &create_nodes);

  void lock_current_nodes(const std::vector<osm_nwr_id_t> &ids);

  std::vector<std::vector<ApiDB_Node_Updater::node_t> >
  build_packages(const std::vector<node_t> &nodes);

  void check_current_node_versions(const std::vector<node_t> &nodes);

  // for if-unused - determine nodes to be excluded from deletion, regardless of
  // their current version
  std::set<osm_nwr_id_t>
  determine_already_deleted_nodes(const std::vector<node_t> &nodes);

  bbox_t calc_node_bbox(const std::vector<osm_nwr_id_t> &ids);

  void update_current_nodes(const std::vector<node_t> &nodes);

  void delete_current_nodes(const std::vector<node_t> &nodes);

  [[nodiscard]] std::vector<osm_nwr_id_t> insert_new_current_node_tags(const std::vector<node_t> &nodes);

  void save_current_nodes_to_history(const std::vector<osm_nwr_id_t> &ids);

  void save_current_node_tags_to_history(const std::vector<osm_nwr_id_t> &ids);

  std::vector<ApiDB_Node_Updater::node_t>
  is_node_still_referenced(const std::vector<node_t> &nodes);

  void delete_current_node_tags(const std::vector<osm_nwr_id_t> &ids);

  Transaction_Manager &m;
  const RequestContext& req_ctx;
  api06::OSMChange_Tracking &ct;

  std::vector<node_t> create_nodes;
  std::vector<node_t> modify_nodes;
  std::vector<node_t> delete_nodes;

  std::set<osm_nwr_signed_id_t> create_placeholder_ids;
};

#endif /* APIDB_NODE_UPDATER */
