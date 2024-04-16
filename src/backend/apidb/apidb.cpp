/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
#include "cgimap/backend/apidb/pgsql_update.hpp"
#include "cgimap/backend.hpp"
#include "cgimap/options.hpp"

#include <memory>

namespace po = boost::program_options;
using std::string;


namespace {
struct apidb_backend : public backend {
  apidb_backend() {
    // clang-format off
    m_options.add_options()
      ("dbname", po::value<string>()->required(), "database name")
      ("host", po::value<string>(), "database server host")
      ("username", po::value<string>(), "database user name")
      ("password", po::value<string>(), "database password")
      ("charset", po::value<string>()->default_value("utf8"),
       "database character set")
      ("readonly", "(obsolete parameter, read only backend is always assumed)")
      ("disable-api-write", "disable API write operations")
      ("dbport", po::value<string>(),
       "database port number or UNIX socket file name")
      ("update-dbname", po::value<string>(),
       "database name to use for API write operations, if different from --dbname")
      ("update-host", po::value<string>(),
       "database server host for API write operations, if different from --host")
      ("update-username", po::value<string>(),
       "database user name for API write operations, if different from --username")
      ("update-password", po::value<string>(),
       "database password for API write operations, if different from --password")
      ("update-charset", po::value<string>(),
       "database character set for API write operations, if different from --charset")
      ("update-dbport", po::value<string>(),
       "database port for API write operations, if different from --dbport");
    // clang-format on
  }
  ~apidb_backend() override = default;

  const string &name() const override { return m_name; }
  const po::options_description &options() const override { return m_options; }

  std::unique_ptr<data_selection::factory> create(const po::variables_map &opts) override {
    return std::make_unique<readonly_pgsql_selection::factory>(opts);
  }

  std::unique_ptr<data_update::factory> create_data_update(const po::variables_map &opts) override {
    return std::make_unique<pgsql_update::factory>(opts);
  }

private:
  string m_name{"apidb"};
  po::options_description m_options{"ApiDB backend options"};
};
}

std::unique_ptr<backend> make_apidb_backend() {
  return std::make_unique<apidb_backend>();
}
