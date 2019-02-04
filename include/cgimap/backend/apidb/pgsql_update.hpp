#ifndef PGSQL_UPDATE_HPP
#define PGSQL_UPDATE_HPP

#include "cgimap/data_update.hpp"
#include "cgimap/backend/apidb/changeset.hpp"
#include "cgimap/backend/apidb/cache.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#include <memory>
#include <pqxx/pqxx>
#include <boost/program_options.hpp>

class pgsql_update : public data_update {

public:
  pgsql_update(pqxx::connection &conn, bool is_readonly);

  ~pgsql_update();

  std::unique_ptr<api06::Changeset_Updater>
  get_changeset_updater(osm_changeset_id_t _changeset, osm_user_id_t _uid);

  std::unique_ptr<api06::Node_Updater>
  get_node_updater(std::shared_ptr<api06::OSMChange_Tracking> _ct);

  std::unique_ptr<api06::Way_Updater>
  get_way_updater(std::shared_ptr<api06::OSMChange_Tracking> _ct);

  std::unique_ptr<api06::Relation_Updater>
  get_relation_updater(std::shared_ptr<api06::OSMChange_Tracking> _ct);

  void commit();

  bool is_readonly();

  /**
   * abstracts the creation of transactions for the
   * data updates.
   */
  class factory : public data_update::factory {
  public:
    factory(const boost::program_options::variables_map &);
    virtual ~factory();
    virtual std::shared_ptr<data_update> make_data_update();

  private:
    pqxx::connection m_connection;
    bool m_readonly;
    pqxx::quiet_errorhandler m_errorhandler;
  };

private:
  Transaction_Manager m;
  bool m_readonly;

};

#endif
