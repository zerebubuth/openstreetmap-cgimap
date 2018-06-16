#ifndef API06_CHANGESET_UPLOAD_NODE_UPDATER
#define API06_CHANGESET_UPLOAD_NODE_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

#include <map>
#include <set>
#include <string>

using TagList = std::map<std::string, std::string>;

class Node_Updater {

public:
  virtual ~Node_Updater() {};

  virtual void add_node(double lat, double lon, osm_changeset_id_t changeset_id,
                        osm_nwr_signed_id_t old_id, const TagList &tags) = 0;

  virtual void modify_node(double lat, double lon,
                           osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                           osm_version_t version, const TagList &tags) = 0;

  virtual void delete_node(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                           osm_version_t version, bool if_unused) = 0;

  virtual void process_new_nodes() = 0;

  virtual void process_modify_nodes() = 0;

  virtual void process_delete_nodes() = 0;

  virtual uint32_t get_num_changes() = 0;

  virtual bbox_t bbox() = 0;
};

#endif /* API06_CHANGESET_UPLOAD_NODE_UPDATER */
