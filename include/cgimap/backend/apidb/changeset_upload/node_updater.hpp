#ifndef APIDB_NODE_UPDATER
#define APIDB_NODE_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#include <set>

class ApiDB_Node_Updater : public Node_Updater {

public:
  ApiDB_Node_Updater(Transaction_Manager &_m,
                     std::shared_ptr<OSMChange_Tracking> _ct);

  virtual ~ApiDB_Node_Updater();

  void add_node(double lat, double lon, osm_changeset_id_t changeset_id,
                osm_nwr_signed_id_t old_id, const TagList &tags);

  void modify_node(double lat, double lon, osm_changeset_id_t changeset_id,
                   osm_nwr_id_t id, osm_version_t version, const TagList &tags);

  void delete_node(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                   osm_version_t version, bool if_unused);

  void process_new_nodes();

  void process_modify_nodes();

  void process_delete_nodes();

  unsigned int get_num_changes();

  bbox_t bbox();

private:
  bbox_t m_bbox;

  struct node_t {
    osm_nwr_id_t id;
    osm_version_t version;
    int64_t lat;
    int64_t lon;
    uint64_t tile;
    osm_changeset_id_t changeset_id;
    osm_nwr_signed_id_t old_id;
    std::vector<std::pair<std::string, std::string> > tags;
    bool if_unused;
  };

  void truncate_temporary_tables();

  void replace_old_ids_in_nodes(
      std::vector<node_t> &create_nodes,
      const std::vector<OSMChange_Tracking::object_id_mapping_t>
          &created_node_id_mapping);

  void check_unique_placeholder_ids(const std::vector<node_t> &create_nodes);

  void insert_new_nodes_to_tmp_table(const std::vector<node_t> &create_nodes);

  void copy_tmp_create_nodes_to_current_nodes();

  void delete_tmp_create_nodes();

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

  void insert_new_current_node_tags(const std::vector<node_t> &nodes);

  void save_current_nodes_to_history(const std::vector<osm_nwr_id_t> &ids);

  void save_current_node_tags_to_history(const std::vector<osm_nwr_id_t> &ids);

  std::vector<ApiDB_Node_Updater::node_t>
  is_node_still_referenced(const std::vector<node_t> &nodes);

  void delete_current_node_tags(const std::vector<osm_nwr_id_t> &ids);

  Transaction_Manager &m;
  std::shared_ptr<OSMChange_Tracking> ct;

  std::vector<node_t> create_nodes;
  std::vector<node_t> modify_nodes;
  std::vector<node_t> delete_nodes;

  std::set<osm_nwr_signed_id_t> create_placedholder_ids;
};

#endif /* APIDB_NODE_UPDATER */
