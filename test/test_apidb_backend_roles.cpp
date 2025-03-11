/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <iostream>
#include <fmt/core.h>

#include <sys/time.h>
#include <cstdio>
#include <memory>

#include "test_database.hpp"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

using roles_t = std::set<osm_user_role_t>;


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


TEST_CASE_METHOD(DatabaseTestsFixture, "test_get_roles_for_user", "[roles][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {


    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2017-02-20T11:41:00Z', 'user_1', true), "
      "  (2, 'user_2@example.com', '', '2017-02-20T11:41:00Z', 'user_2', true), "
      "  (3, 'user_3@example.com', '', '2017-02-20T11:41:00Z', 'user_3', true); "

      "INSERT INTO user_roles (id, user_id, role, granter_id) "
      "VALUES "
      "  (1, 1, 'administrator', 1), "
      "  (2, 1, 'moderator', 1), "
      "  (3, 2, 'moderator', 1), "
      "  (4, 2, 'importer', 1); ");

  }

  using enum osm_user_role_t;

  // user 3 has no roles -> should return empty set
  REQUIRE(roles_t() == sel->get_roles_for_user(3));

  // user 2 is a moderator and importer
  REQUIRE(roles_t({moderator, importer}) == sel->get_roles_for_user(2));

  // user 1 is an administrator and a moderator
  REQUIRE(roles_t({moderator, administrator}) == sel->get_roles_for_user(1));
}

int main(int argc, char *argv[]) {
  Catch::Session session;

  std::filesystem::path test_db_sql{ "test/structure.sql" };

  using namespace Catch::clara;
  auto cli =
      session.cli()
      | Opt(test_db_sql,
            "db-schema")    // bind variable to a new option, with a hint string
            ["--db-schema"] // the option names it will respond to
      ("test database schema file"); // description string for the help output

  session.cli(cli);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0)
    return returnCode;

  if (!test_db_sql.empty())
    DatabaseTestsFixture::setTestDatabaseSchema(test_db_sql);

  return session.run();
}
