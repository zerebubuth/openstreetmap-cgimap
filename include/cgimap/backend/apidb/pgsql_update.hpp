/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef PGSQL_UPDATE_HPP
#define PGSQL_UPDATE_HPP

#include "cgimap/data_update.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/api06/changeset_upload/changeset_updater.hpp"

#include <memory>
#include <pqxx/pqxx>
#include <boost/program_options.hpp>

struct RequestContext;

class pgsql_update : public data_update {

public:
  pgsql_update(Transaction_Owner_Base& to, bool is_readonly);

  ~pgsql_update() override = default;

  std::unique_ptr<api06::Changeset_Updater>
  get_changeset_updater(const RequestContext& ctx, osm_changeset_id_t _changeset) override;

  std::unique_ptr<api06::Node_Updater>
  get_node_updater(const RequestContext& ctx, api06::OSMChange_Tracking &ct) override;

  std::unique_ptr<api06::Way_Updater>
  get_way_updater(const RequestContext& ctx, api06::OSMChange_Tracking &ct) override;

  std::unique_ptr<api06::Relation_Updater>
  get_relation_updater(const RequestContext& ctx, api06::OSMChange_Tracking &ct) override;

  void commit() override;

  bool is_api_write_disabled() const override;

  uint32_t get_rate_limit(osm_user_id_t uid) override;

  uint64_t get_bbox_size_limit(osm_user_id_t uid) override;

  /**
   * abstracts the creation of transactions for the
   * data updates.
   */
  class factory : public data_update::factory {
  public:
    factory(const boost::program_options::variables_map &);
    ~factory() override = default;
    std::unique_ptr<data_update> make_data_update(Transaction_Owner_Base& to) override;
    std::unique_ptr<Transaction_Owner_Base> get_default_transaction() override;
    std::unique_ptr<Transaction_Owner_Base> get_read_only_transaction() override;

  private:
    pqxx::connection m_connection;
    bool m_api_write_disabled;
    pqxx::quiet_errorhandler m_errorhandler;
    std::set<std::string> m_prep_stmt;  // keeps track of already prepared statements
  };

private:
  Transaction_Manager m;
  bool m_readonly;
};

#endif
