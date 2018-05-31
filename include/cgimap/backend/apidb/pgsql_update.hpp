#ifndef PGSQL_UPDATE_HPP
#define PGSQL_UPDATE_HPP

#include "cgimap/data_update.hpp"
#include "cgimap/backend/apidb/changeset.hpp"
#include "cgimap/backend/apidb/cache.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include <pqxx/pqxx>
#include <boost/program_options.hpp>

class pgsql_update : public data_update {

public:
  pgsql_update(pqxx::connection &conn);

  ~pgsql_update();

  std::unique_ptr<Changeset_Updater>
  get_changeset_updater(osm_changeset_id_t _changeset, osm_user_id_t _uid);

  std::unique_ptr<Node_Updater>
  get_node_updater(std::shared_ptr<OSMChange_Tracking> _ct);

  std::unique_ptr<Way_Updater>
  get_way_updater(std::shared_ptr<OSMChange_Tracking> _ct);

  std::unique_ptr<Relation_Updater>
  get_relation_updater(std::shared_ptr<OSMChange_Tracking> _ct);

  void commit();

  /**
   * abstracts the creation of transactions for the
   * data updates.
   */
  class factory : public data_update::factory {
  public:
    factory(const boost::program_options::variables_map &);
    virtual ~factory();
    virtual boost::shared_ptr<data_update> make_data_update();

  private:
    pqxx::connection m_connection;
#if PQXX_VERSION_MAJOR >= 4
    pqxx::quiet_errorhandler m_errorhandler;
#endif
  };

private:
  Transaction_Manager m;

};

#endif
