#ifndef API06_CHANGESET_UPLOAD_WAY_UPDATER
#define API06_CHANGESET_UPLOAD_WAY_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>



namespace api06 {

using TagList = std::map<std::string, std::string>;
using WayNodeList = std::vector<osm_nwr_signed_id_t>;

/*  Way operations
 *
 */
class Way_Updater {

public:
  virtual ~Way_Updater() = default;

  virtual void add_way(osm_changeset_id_t changeset_id,
                       osm_nwr_signed_id_t old_id, const WayNodeList &nodes,
                       const TagList &tags) = 0;

  virtual void modify_way(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                          osm_version_t version, const WayNodeList &nodes,
                          const TagList &tags) = 0;

  virtual void delete_way(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                          osm_version_t version, bool if_unused) = 0;

  virtual void process_new_ways() = 0;

  virtual void process_modify_ways() = 0;

  virtual void process_delete_ways() = 0;

  virtual uint32_t get_num_changes() = 0;

  virtual bbox_t bbox() = 0;
};

} // namespace api06

#endif /* API06_CHANGESET_UPLOAD_WAY_UPDATER */
