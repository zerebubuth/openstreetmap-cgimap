#ifndef DATA_UPDATE_HPP
#define DATA_UPDATE_HPP

#include "cgimap/output_formatter.hpp"
#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/changeset_updater.hpp"
#include "cgimap/api06/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/relation_updater.hpp"
#include "cgimap/api06/changeset_upload/way_updater.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

/**
 */
class data_update {
public:
  virtual ~data_update() {};

  virtual std::unique_ptr<Changeset_Updater>
  get_changeset_updater(osm_changeset_id_t _changeset, osm_user_id_t _uid) = 0;

  virtual std::unique_ptr<Node_Updater>
  get_node_updater(std::shared_ptr<OSMChange_Tracking> _ct) = 0;

  virtual std::unique_ptr<Way_Updater>
  get_way_updater(std::shared_ptr<OSMChange_Tracking> _ct) = 0;

  virtual std::unique_ptr<Relation_Updater>
  get_relation_updater(std::shared_ptr<OSMChange_Tracking> _ct) = 0;

  virtual void
  commit() = 0;

  /**
   * factory for the creation of data updates. this abstracts away
   * the creation process of transactions, and allows some up-front
   * work to be done. for example, setting up prepared statements on
   * a database connection.
   */
  struct factory {
    virtual ~factory() {};

    /// get a handle to a selection which can be used to build up
    /// a working set of data.
    virtual boost::shared_ptr<data_update> make_data_update() = 0;
  };
};

typedef boost::shared_ptr<data_update::factory> factory_update_ptr;
typedef boost::shared_ptr<data_update> data_update_ptr;

#endif /* DATA_UPDATE_HPP */
