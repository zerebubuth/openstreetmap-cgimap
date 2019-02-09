#ifndef DATA_UPDATE_HPP
#define DATA_UPDATE_HPP

#include "cgimap/output_formatter.hpp"
#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/changeset_updater.hpp"
#include "cgimap/api06/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/relation_updater.hpp"
#include "cgimap/api06/changeset_upload/way_updater.hpp"

#include <memory>
#include <vector>

/**
 */
class data_update {
public:
  virtual ~data_update() = default;

  virtual std::unique_ptr<api06::Changeset_Updater>
  get_changeset_updater(osm_changeset_id_t _changeset, osm_user_id_t _uid) = 0;

  virtual std::unique_ptr<api06::Node_Updater>
  get_node_updater(std::shared_ptr<api06::OSMChange_Tracking> _ct) = 0;

  virtual std::unique_ptr<api06::Way_Updater>
  get_way_updater(std::shared_ptr<api06::OSMChange_Tracking> _ct) = 0;

  virtual std::unique_ptr<api06::Relation_Updater>
  get_relation_updater(std::shared_ptr<api06::OSMChange_Tracking> _ct) = 0;

  virtual void
  commit() = 0;

  virtual bool is_readonly() = 0;

  /**
   * factory for the creation of data updates. this abstracts away
   * the creation process of transactions, and allows some up-front
   * work to be done. for example, setting up prepared statements on
   * a database connection.
   */
  struct factory {
    virtual ~factory() = default;

    /// get a handle to a selection which can be used to build up
    /// a working set of data.
    virtual std::shared_ptr<data_update> make_data_update() = 0;
  };
};

using factory_update_ptr = std::shared_ptr<data_update::factory>;
using data_update_ptr = std::shared_ptr<data_update>;

#endif /* DATA_UPDATE_HPP */
