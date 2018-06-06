#ifndef APIDB_WAY_UPDATER
#define APIDB_WAY_UPDATER

#include "cgimap/types.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload/way_updater.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <set>

/*  Way operations
 *
 */

class ApiDB_Way_Updater : public Way_Updater {

public:
  ApiDB_Way_Updater(Transaction_Manager &_m,
                    std::shared_ptr<OSMChange_Tracking> _ct);

  virtual ~ApiDB_Way_Updater();

  void add_way(osm_changeset_id_t changeset_id, osm_nwr_signed_id_t old_id,
               const WayNodeList &nodes, const TagList &tags);

  void modify_way(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                  osm_version_t version, const WayNodeList &nodes,
                  const TagList &tags);

  void delete_way(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                  osm_version_t version, bool if_unused);

  void process_new_ways();

  void process_modify_ways();

  void process_delete_ways();

  unsigned int get_num_changes();

  bbox_t bbox();

private:
  bbox_t m_bbox;

  struct way_node_t {
    osm_nwr_id_t node_id;
    long sequence_id;
    osm_nwr_signed_id_t old_node_id;
  };

  struct way_t {
    osm_nwr_id_t id;
    osm_version_t version;
    osm_changeset_id_t changeset_id;
    osm_nwr_signed_id_t old_id;
    std::vector<std::pair<std::string, std::string> > tags;
    std::vector<way_node_t> way_nodes;
    bool if_unused;
  };

  void truncate_temporary_tables();

  /*
   * Set id field based on old_id -> id mapping
   *
   */

  void replace_old_ids_in_ways(
      std::vector<way_t> &ways,
      const std::vector<OSMChange_Tracking::object_id_mapping_t>
          &created_node_id_mapping,
      const std::vector<OSMChange_Tracking::object_id_mapping_t>
          &created_way_id_mapping);

  void check_unique_placeholder_ids(const std::vector<way_t> &create_ways);

  void insert_new_ways_to_tmp_table(const std::vector<way_t> &create_ways);

  void copy_tmp_create_ways_to_current_ways();

  void delete_tmp_create_ways();

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

  void insert_new_current_way_tags(const std::vector<way_t> &ways);

  void insert_new_current_way_nodes(const std::vector<way_t> &ways);

  void save_current_ways_to_history(const std::vector<osm_nwr_id_t> &ids);

  void save_current_way_nodes_to_history(const std::vector<osm_nwr_id_t> &ids);

  void save_current_way_tags_to_history(const std::vector<osm_nwr_id_t> &ids);

  std::vector<ApiDB_Way_Updater::way_t>
  is_way_still_referenced(const std::vector<way_t> &ways);

  void delete_current_way_tags(const std::vector<osm_nwr_id_t> &ids);

  void delete_current_way_nodes(std::vector<osm_nwr_id_t> ids);

  Transaction_Manager &m;
  std::shared_ptr<OSMChange_Tracking> ct;

  std::vector<way_t> create_ways;
  std::vector<way_t> modify_ways;
  std::vector<way_t> delete_ways;

  std::set<osm_nwr_signed_id_t> create_placedholder_ids;
};

#endif /* APIDB_WAY_UPDATER */
