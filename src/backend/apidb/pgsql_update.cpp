/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/pgsql_update.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"

#include <functional>
#include <sstream>


namespace po = boost::program_options;

namespace {
std::string connect_db_str(const po::variables_map &options) {
  // build the connection string.
  std::ostringstream ostr;

  if (options.count("dbname") == 0 &&
      options.count("update-dbname") == 0) {
    throw std::runtime_error("Must provide either one of --dbname or "
                             "--update-dbname to configure database "
                             "name for update (API write) connections.");
  }

#define CONNOPT(a,b)                                                    \
  if (options.count("update-" a)) {                                     \
    ostr << " " << (b "=") << options["update-" a].as<std::string>();   \
  } else if (options.count(a)) {                                        \
    ostr << " " << (b "=") << options[a].as<std::string>();             \
  }

  CONNOPT("dbname", "dbname");
  CONNOPT("host", "host");
  CONNOPT("username", "user");
  CONNOPT("password", "password");
  CONNOPT("dbport", "port");

#undef CONNOPT
  return ostr.str();
}

} // anonymous namespace

pgsql_update::pgsql_update(Transaction_Owner_Base& to, bool readonly)
    : m{ to }, 
      m_readonly{ readonly } {

  if (is_api_write_disabled())
    return;

  m.exec(R"(CREATE TEMPORARY TABLE tmp_create_nodes 
      (
        id bigint NOT NULL DEFAULT nextval('current_nodes_id_seq'::regclass),
        latitude integer NOT NULL,
        longitude integer NOT NULL,
        changeset_id bigint NOT NULL,
        visible boolean NOT NULL DEFAULT true,
        "timestamp" timestamp without time zone NOT NULL DEFAULT (now() at time zone 'utc'),
        tile bigint NOT NULL,
        version bigint NOT NULL DEFAULT 1,
        old_id bigint NOT NULL UNIQUE,
        PRIMARY KEY (id))
        ON COMMIT DROP
      )");

  m.exec(R"(CREATE TEMPORARY TABLE tmp_create_ways 
      (
        id bigint NOT NULL DEFAULT nextval('current_ways_id_seq'::regclass),
        changeset_id bigint NOT NULL,
        visible boolean NOT NULL DEFAULT true,
        "timestamp" timestamp without time zone NOT NULL DEFAULT (now() at time zone 'utc'),
        version bigint NOT NULL DEFAULT 1,
        old_id bigint NOT NULL UNIQUE,
        PRIMARY KEY (id))
        ON COMMIT DROP
     )");

  m.exec(R"(CREATE TEMPORARY TABLE tmp_create_relations 
     (
        id bigint NOT NULL DEFAULT nextval('current_relations_id_seq'::regclass),
        changeset_id bigint NOT NULL,
        visible boolean NOT NULL DEFAULT true,
        "timestamp" timestamp without time zone NOT NULL DEFAULT (now() at time zone 'utc'),
        version bigint NOT NULL DEFAULT 1,
        old_id bigint NOT NULL UNIQUE,
        PRIMARY KEY (id))
        ON COMMIT DROP
     )");
}

bool pgsql_update::is_api_write_disabled() const {
  return m_readonly;
}

std::unique_ptr<api06::Changeset_Updater>
pgsql_update::get_changeset_updater(osm_changeset_id_t changeset, osm_user_id_t uid)
{
  return std::make_unique<ApiDB_Changeset_Updater>(m, changeset, uid);
}

std::unique_ptr<api06::Node_Updater>
pgsql_update::get_node_updater(api06::OSMChange_Tracking &ct)
{
  return std::make_unique<ApiDB_Node_Updater>(m, ct);
}

std::unique_ptr<api06::Way_Updater>
pgsql_update::get_way_updater(api06::OSMChange_Tracking &ct)
{
  return std::make_unique<ApiDB_Way_Updater>(m, ct);
}

std::unique_ptr<api06::Relation_Updater>
pgsql_update::get_relation_updater(api06::OSMChange_Tracking &ct)
{
  return std::make_unique<ApiDB_Relation_Updater>(m, ct);
}

void pgsql_update::commit() {
  m.commit();
}

uint32_t pgsql_update::get_rate_limit(osm_user_id_t uid)
{
  m.prepare("api_rate_limit",
    R"(SELECT * FROM api_rate_limit($1) LIMIT 1 )");

  auto res = m.exec_prepared("api_rate_limit", uid);

  if (res.size() != 1) {
    throw http::server_error("api_rate_limit db function did not return any data");
  }

  auto row = res[0];
  auto rate_limit = row[0].as<int32_t>();

  return std::max(0, rate_limit);
}

uint64_t pgsql_update::get_bbox_size_limit(osm_user_id_t uid)
{
  {
    m.prepare("api_size_limit",
      R"(SELECT * FROM api_size_limit($1) LIMIT 1 )");

    auto res = m.exec_prepared("api_size_limit", uid);

    if (res.size() != 1) {
      throw http::server_error("api_size_limit db function did not return any data");
    }

    auto row = res[0];
    auto bbox_size_limit = row[0].as<int64_t>();

    return std::max(bbox_size_limit, 0l);
  }
}


pgsql_update::factory::factory(const po::variables_map &opts)
  : m_connection(connect_db_str(opts)), 
    m_api_write_disabled(false),
    m_errorhandler(m_connection) {

  check_postgres_version(m_connection);

  // set the connections to use the appropriate charset.
  std::string db_charset = opts["charset"].as<std::string>();
  if (opts.count("update-charset")) {
     db_charset = opts["update-charset"].as<std::string>();
  }
  m_connection.set_client_encoding(db_charset);

  // set the connection to readonly transaction, if disable-api-write flag is set
  if (opts.count("disable-api-write") != 0) {
    m_api_write_disabled = true;
#if PQXX_VERSION_MAJOR < 7
    m_connection.set_variable("default_transaction_read_only", "true");
#else
    m_connection.set_session_var("default_transaction_read_only", "true");
#endif
  }
}

std::unique_ptr<data_update>
pgsql_update::factory::make_data_update(Transaction_Owner_Base& to) {
  return std::make_unique<pgsql_update>(to, m_api_write_disabled);
}

std::unique_ptr<Transaction_Owner_Base>
pgsql_update::factory::get_default_transaction()
{
  return std::make_unique<Transaction_Owner_ReadWrite>(std::ref(m_connection), m_prep_stmt);
}

std::unique_ptr<Transaction_Owner_Base>
pgsql_update::factory::get_read_only_transaction()
{
  return std::make_unique<Transaction_Owner_ReadOnly>(std::ref(m_connection), m_prep_stmt);
}

