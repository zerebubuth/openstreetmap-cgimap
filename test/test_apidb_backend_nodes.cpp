/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <stdexcept>
#include <fmt/core.h>

#include <sys/time.h>
#include <cstdio>

#include "cgimap/backend/apidb/utils.hpp"

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


TEST_CASE_METHOD( DatabaseTestsFixture, "test_single_nodes", "[nodes][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true), "
      "  (2, 'user_2@example.com', '', '2013-11-14T02:10:00Z', 'user_2', false); "

      "INSERT INTO changesets (id, user_id, created_at, closed_at) "
      "VALUES "
      "  (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
      "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'), "
      "  (4, 2, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"

      "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) "
      " VALUES "
      "  (1,       0,       0, 1, true,  '2013-11-14T02:10:00Z', 3221225472, 1), "
      "  (2, 1000000, 1000000, 1, true,  '2013-11-14T02:10:01Z', 3221227032, 1), "
      "  (3,       0,       0, 2, false, '2015-03-02T18:27:00Z', 3221225472, 2), "
      "  (4,       0,       0, 4, true,  '2015-03-02T19:25:00Z', 3221225472, 1);"
      );
  }

  SECTION("Perform single node checks") {

    std::vector<osm_nwr_id_t> ids{1,2,3,4};

    REQUIRE (sel->select_nodes(ids) == 4); // 4 ids read from database
    REQUIRE (sel->select_nodes(ids) == 0); // Re-selecting 4 nodes failed, values should be still in buffer

    REQUIRE (sel->check_node_visibility(1) == data_selection::exists);
    REQUIRE (sel->check_node_visibility(2) == data_selection::exists);
    REQUIRE (sel->check_node_visibility(3) == data_selection::deleted);
    REQUIRE (sel->check_node_visibility(4) == data_selection::exists);
    REQUIRE (sel->check_node_visibility(5) == data_selection::non_exist);

    test_formatter f;
    sel->write_nodes(f);

    REQUIRE (f.m_nodes.size() == 4);

    REQUIRE(
      test_formatter::node_t(
        element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
        0.0, 0.0,
        tags_t()
        ) == f.m_nodes[0]);

    REQUIRE(
      test_formatter::node_t(
        element_info(2, 1, 1, "2013-11-14T02:10:01Z", 1, std::string("user_1"), true),
        0.1, 0.1,
        tags_t()
        ) == f.m_nodes[1]);

    REQUIRE(
      test_formatter::node_t(
        element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
        0.0, 0.0,
        tags_t()
        ) == f.m_nodes[2]);

    REQUIRE(
      test_formatter::node_t(
        element_info(4, 1, 4, "2015-03-02T19:25:00Z", {}, {}, true),
        0.0, 0.0,
        tags_t()
        ) == f.m_nodes[3]);
  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_dup_nodes", "[nodes][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {
    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

      "INSERT INTO changesets (id, user_id, created_at, closed_at) "
      "VALUES "
      "  (1, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z');"

      "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) "
      " VALUES "
      "  (1,       0,       0, 1, true,  '2013-11-14T02:10:00Z', 3221225472, 1);"
      );
  }

  SECTION("Perform duplicate node checks") {

    REQUIRE (sel->check_node_visibility(1) == data_selection::exists);

    std::vector<osm_nwr_id_t> ids{1,1,1};

    REQUIRE (sel->select_nodes(ids) == 1);
    REQUIRE (sel->select_nodes(ids) == 0); // Re-selecting the same node failed

    REQUIRE (sel->check_node_visibility(1) == data_selection::exists);

    test_formatter f;
    sel->write_nodes(f);

    REQUIRE (f.m_nodes.size() == 1);

    REQUIRE(
      test_formatter::node_t(
        element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
        0.0, 0.0,
        tags_t()
        ) == f.m_nodes[0]);
  }
}

TEST_CASE("test_psql_array_to_vector", "[nodb]") {

  std::string test;
  std::vector<std::string> actual_values;
  std::vector<std::string> values;

  SECTION("NULL") {
    test = "{NULL}";
    values = psql_array_to_vector(test);
    REQUIRE (values == actual_values);
  }

  SECTION("Two values") {
    test = "{1,2}";
    values = psql_array_to_vector(test);
    actual_values = {"1", "2"};
    REQUIRE (values == actual_values);
  }

  SECTION("Two strings") {
    test = "{\"TEST\",TEST123}";
    values = psql_array_to_vector(test);
    actual_values = { "TEST", "TEST123" };
    REQUIRE (values == actual_values);
  }

  SECTION("Complex pattern") {
    test = R"({"},\"",",{}}\\"})";
    values = psql_array_to_vector(test);
    actual_values.emplace_back("},\"");
    actual_values.emplace_back(",{}}\\");
    REQUIRE (values == actual_values);
  }

  SECTION("test with semicolon in key") {
    test = R"({use_sidepath,secondary,3,1,yes,50,"Rijksweg Noord",asphalt,left|through;right})";
    values = psql_array_to_vector(test);
    actual_values.emplace_back("use_sidepath");
    actual_values.emplace_back("secondary");
    actual_values.emplace_back("3");
    actual_values.emplace_back("1");
    actual_values.emplace_back("yes");
    actual_values.emplace_back("50");
    actual_values.emplace_back("Rijksweg Noord");
    actual_values.emplace_back("asphalt");
    actual_values.emplace_back("left|through;right");
    REQUIRE (values == actual_values);
  }
}

TEST_CASE("psql_array_ids_to_vector", "[nodb]") {

  std::string test;
  std::vector<int64_t> actual_values;
  std::vector<int64_t> values;

  SECTION("NULL") {
    test = "{NULL}";
    values = psql_array_ids_to_vector<int64_t>(test);
    REQUIRE (values == actual_values);
  }

  SECTION("Empty string") {
    test = "";
    values = psql_array_ids_to_vector<int64_t>(test);
    REQUIRE (values == actual_values);
  }

  SECTION("One value") {
    test = "{1}";
    values = psql_array_ids_to_vector<int64_t>(test);
    actual_values = {1};
    REQUIRE (values == actual_values);
  }

  SECTION("Two values") {
    test = "{1,-2}";
    values = psql_array_ids_to_vector<int64_t>(test);
    actual_values = {1, -2};
    REQUIRE (values == actual_values);
  }

  SECTION("Invalid string") {
    test = "{1,}";
    REQUIRE_THROWS_AS(psql_array_ids_to_vector<int64_t>(test), std::runtime_error);
  }
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
