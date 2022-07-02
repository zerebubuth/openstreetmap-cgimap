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
  pgsql_update(Transaction_Owner_Base& to, bool is_readonly);

  ~pgsql_update();

  std::unique_ptr<api06::Changeset_Updater>
  get_changeset_updater(osm_changeset_id_t _changeset, osm_user_id_t _uid);

  std::unique_ptr<api06::Node_Updater>
  get_node_updater(api06::OSMChange_Tracking &ct);

  std::unique_ptr<api06::Way_Updater>
  get_way_updater(api06::OSMChange_Tracking &ct);

  std::unique_ptr<api06::Relation_Updater>
  get_relation_updater(api06::OSMChange_Tracking &ct);

  void commit();

  bool is_api_write_disabled() const;

  /**
   * abstracts the creation of transactions for the
   * data updates.
   */
  class factory : public data_update::factory {
  public:
    factory(const boost::program_options::variables_map &);
    virtual ~factory();
    std::unique_ptr<data_update> make_data_update(Transaction_Owner_Base& to) override;
    std::unique_ptr<Transaction_Owner_Base> get_default_transaction() override;
    std::unique_ptr<Transaction_Owner_Base> get_read_only_transaction() override;

  private:
    pqxx::connection m_connection;
    bool m_api_write_disabled;
    pqxx::quiet_errorhandler m_errorhandler;
  };

private:
  Transaction_Manager m;
  bool m_readonly;

};

#endif
