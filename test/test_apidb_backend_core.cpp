/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/process_request.hpp"
#include "cgimap/time.hpp"
#include "cgimap/util.hpp"

#include <stdexcept>
#include <fmt/core.h>

#include <sys/time.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

#include "test_core_helper.hpp"
#include "test_request.hpp"
#include "xmlparser.hpp"
#include "test_apidb_importer.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

namespace {

class DatabaseTestsFixture
{
public:
  static void setTestDatabaseSchema(const std::filesystem::path& db_sql) {
    test_db_sql = db_sql;
  }

protected:
  DatabaseTestsFixture() = default;
  inline static std::filesystem::path test_db_sql{"test/structure.sql"};
  static test_database tdb;
};

test_database DatabaseTestsFixture::tdb{};

struct CGImapListener : Catch::TestEventListenerBase, DatabaseTestsFixture {

    using TestEventListenerBase::TestEventListenerBase; // inherit constructor

    void testRunStarting( Catch::TestRunInfo const& testRunInfo ) override {
      // load database schema when starting up tests
      tdb.setup(test_db_sql);
    }

    void testCaseStarting( Catch::TestCaseInfo const& testInfo ) override {
      tdb.testcase_starting();
    }

    void testCaseEnded( Catch::TestCaseStats const& testCaseStats ) override {
      tdb.testcase_ended();
    }
};

CATCH_REGISTER_LISTENER( CGImapListener )

} // anonymous namespace


fs::path test_directory{};

std::vector<fs::path> get_test_cases() {
  std::vector<fs::path> test_cases;

  for (const auto& entry : fs::directory_iterator(test_directory)) {
    fs::path filename = entry.path();
    if (filename.extension() == ".case") {
      test_cases.push_back(filename);
    }
  }
  return test_cases;
}

TEST_CASE_METHOD( DatabaseTestsFixture, "readonly_pgsql core", "[core][db]" ) {

  SECTION("Initialize test data") {

    user_roles_t user_roles;
    oauth2_tokens oauth2_tokens;

    if (test_directory.empty()) {
      FAIL("No test directory specified. Missing --test-directory command line option.");
    }

    fs::path data_file = test_directory / "data.osm";
    fs::path oauth2_file = test_directory / "oauth2.json";
    fs::path roles_file = test_directory / "roles.json";

    if (!fs::is_directory(test_directory)) {
      FAIL("Test directory does not exist.");
    }

    if (!fs::is_regular_file(data_file)) {
      FAIL("data.osm file does not exist in given test directory.");
    }

    REQUIRE_NOTHROW(user_roles = get_user_roles(roles_file));
    REQUIRE_NOTHROW(oauth2_tokens = get_oauth2_tokens(oauth2_file));

    auto database = parse_xml(data_file.c_str());

    auto txn = tdb.get_data_update_factory()->get_default_transaction();
    auto m = Transaction_Manager(*txn);

    populate_database(m, *database, user_roles, oauth2_tokens);

    m.commit();
  }

  SECTION("Execute test cases") {

    null_rate_limiter limiter;
    routes route;

    auto sel_factory = tdb.get_data_selection_factory();

    auto test_cases = get_test_cases();
    if(test_cases.empty()) {
      FAIL("No test cases found in the test directory.");
    }

    // Execute actual test cases
    for (fs::path test_case : test_cases) {
      SECTION("Execute single testcase " + test_case.filename().string()) {
        std::string generator = fmt::format(PACKAGE_STRING " (test {})", test_case.string());
        test_request req;

        // set up request headers from test case
        std::ifstream in(test_case, std::ios::binary);
        setup_request_headers(req, in);

        // execute the request
        REQUIRE_NOTHROW(process_request(req, limiter, generator, route, *sel_factory, nullptr));

        CAPTURE(req.body().str());
        REQUIRE_NOTHROW(check_response(in, req.buffer()));
      }
    }
  }
}

int main(int argc, char *argv[]) {
  Catch::Session session;

  std::filesystem::path test_db_sql{ "test/structure.sql" };

  using namespace Catch::clara;
  auto cli =
      session.cli()
      | Opt(test_db_sql,
            "db-schema")
            ["--db-schema"]
      ("test database schema file")
      | Opt(test_directory,
        "test-directory")
        ["--test-directory"]
        ("test case directory");

  session.cli(cli);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0)
    return returnCode;

  if (!test_db_sql.empty())
    DatabaseTestsFixture::setTestDatabaseSchema(test_db_sql);

  return session.run();
}
