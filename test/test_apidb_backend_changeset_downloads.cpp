/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <fmt/core.h>

#include <sys/time.h>
#include <cstdio>

#include "test_formatter.hpp"
#include "test_database.hpp"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>


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


TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_select_node", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2017-03-19T19:13:00Z', 'user_1', true); "

      "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
      "VALUES "
      "  (1, 1, '2017-03-19T19:13:00Z', '2017-03-19T19:13:00Z', 1);"

      "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) "
      " VALUES "
      "  (1, 90000000, 90000000, 1, true,  '2017-03-19T19:13:00Z', 3229120632, 1); "

      "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
      "                   \"timestamp\", tile, version, redaction_id) "
      "VALUES "
      "  (1, 90000000, 90000000, 1, true,  '2017-03-19T19:13:00Z', 3229120632, 1, NULL);"
      );

  }

  SECTION("Perform node checks") {

    int num = sel->select_historical_by_changesets({1});
    REQUIRE(num == 1); // should have selected one element from changeset 1

    test_formatter f;
    sel->write_nodes(f);
    REQUIRE(f.m_nodes.size() == 1); // should have written one node from changeset 1

    REQUIRE(
      test_formatter::node_t(
        element_info(1, 1, 1, "2017-03-19T19:13:00Z", 1, std::string("user_1"), true),
        9.0, 9.0,
        tags_t()) == f.m_nodes.front()); // node 1 in changeset 1

  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_select_way", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2017-03-19T19:57:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
    "VALUES "
    "  (1, 1, '2017-03-19T19:13:00Z', '2017-03-19T19:57:00Z', 1),"
    "  (2, 1, '2017-03-19T19:13:00Z', '2017-03-19T19:57:00Z', 2);"

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) "
    " VALUES "
    "  (1, 90000000, 90000000, 1, true,  '2017-03-19T19:57:00Z', 3229120632, 1); "

    "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
    "                   \"timestamp\", tile, version, redaction_id) "
    "VALUES "
    "  (1, 90000000, 90000000, 1, true,  '2017-03-19T19:57:00Z', 3229120632, 1, NULL);"

    "INSERT INTO ways (way_id, changeset_id, \"timestamp\", visible, "
    "                  version, redaction_id) "
    "VALUES "
    "  (1, 2, '2017-03-19T19:57:00Z', true, 2, NULL), "
    "  (1, 2, '2017-03-19T19:57:00Z', true, 1, NULL); "

    "INSERT INTO way_nodes (way_id, version, node_id, sequence_id) "
    "VALUES "
    "  (1, 2, 1, 1), "
    "  (1, 1, 1, 1); "
    );
  }

  SECTION("Perform way checks") {

    int num = sel->select_historical_by_changesets({2});
    REQUIRE(num == 2); // number of ways (2) selected from changeset 1

    test_formatter f;
    sel->write_ways(f);
    REQUIRE(f.m_ways.size() == 2); // number of ways (2) written from changeset 1

    REQUIRE(
        test_formatter::way_t(
        element_info(1, 1, 2, "2017-03-19T19:57:00Z", 1, std::string("user_1"), true),
        nodes_t{1},
        tags_t()) == f.m_ways[0]); // way 1, version 1 in changeset 1

    REQUIRE(
        test_formatter::way_t(
        element_info(1, 2, 2, "2017-03-19T19:57:00Z", 1, std::string("user_1"), true),
        nodes_t{1},
        tags_t()) == f.m_ways[1]); // way 1, version 2 in changeset 1
  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_select_relation", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2017-03-19T20:15:00Z', 'user_1', true); "

      "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
      "VALUES "
      "  (1, 1, '2017-03-19T19:13:00Z', '2017-03-19T20:15:00Z', 1),"
      "  (2, 1, '2017-03-19T19:13:00Z', '2017-03-19T20:15:00Z', 1);"

      "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) "
      " VALUES "
      "  (1, 90000000, 90000000, 1, true,  '2017-03-19T20:15:00Z', 3229120632, 1); "

      "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
      "                   \"timestamp\", tile, version, redaction_id) "
      "VALUES "
      "  (1, 90000000, 90000000, 1, true,  '2017-03-19T20:15:00Z', 3229120632, 1, NULL);"

      "INSERT INTO relations (relation_id, changeset_id, \"timestamp\", "
      "                       visible, version, redaction_id) "
      "VALUES "
      "  (1, 2, '2017-03-19T20:15:00Z', true, 1, NULL); "

      "INSERT INTO relation_members (relation_id, member_type, member_id, "
      "                              member_role, sequence_id, version) "
      "VALUES "
      "  (1, 'Node', 1, 'foo', 1, 1); "
      );

  }

  SECTION("Perform relation checks") {

    int num = sel->select_historical_by_changesets({2});
    REQUIRE(num == 1); // number of relations (1) selected from changeset 1

    test_formatter f;
    sel->write_relations(f);
    REQUIRE(f.m_relations.size() == 1); // number of relations (1) written from changeset 1

    REQUIRE(
      test_formatter::relation_t(
        element_info(1, 1, 2, "2017-03-19T20:15:00Z", 1, std::string("user_1"), true),
        members_t{member_info(element_type::node, 1, "foo")},
        tags_t()) == f.m_relations.front());  // relation 1, version 1 in changeset 1
  }
}

TEST_CASE_METHOD( DatabaseTestsFixture, "test_changeset_redacted", "[changeset][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    tdb.run_sql(
      "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
      "VALUES "
      "  (1, 'user_1@example.com', '', '2017-03-19T20:18:00Z', 'user_1', true); "

      "INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes) "
      "VALUES "
      "  (1, 1, '2017-03-19T20:18:00Z', '2017-03-19T20:18:00Z', 1),"
      "  (2, 1, '2017-03-19T20:18:00Z', '2017-03-19T20:18:00Z', 1),"
      "  (3, 1, '2017-03-19T20:18:00Z', '2017-03-19T20:18:00Z', 1);"

      "INSERT INTO redactions (id, title, description, created_at, updated_at, user_id) "
      "VALUES "
      "  (1, 'test redaction', 'test redaction description', '2017-03-19T20:18:00Z', '2017-03-19T20:18:00Z', 1); "

      "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) "
      " VALUES "
      "  (1, 0, 0, 3, true,  '2017-03-19T20:18:00Z', 3221225472, 3); "

      "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
      "                   \"timestamp\", tile, version, redaction_id) "
      "VALUES "
      "  (1, 0, 0, 1, true,  '2017-03-19T20:18:00Z', 3221225472, 1, NULL),"
      "  (1, 0, 0, 2, true,  '2017-03-19T20:18:00Z', 3221225472, 2, 1),"
      "  (1, 0, 0, 3, true,  '2017-03-19T20:18:00Z', 3221225472, 3, NULL);"
      );
  }

  SECTION("Perform redaction checks") {

    {
      int num = sel->select_historical_by_changesets({2});
      REQUIRE(num == 0); // number of elements (0) selected by regular user from changeset 2

      test_formatter f;
      sel->write_nodes(f);
      REQUIRE(f.m_nodes.size() == 0); // number of nodes (0) written for regular user from changeset 2
    }

    // as a moderator, should have all redacted elements shown.
    sel->set_redactions_visible(true);
    {
      int num = sel->select_historical_by_changesets({2});
      REQUIRE(num ==  1);  // number of elements (1) selected by moderator from changeset 2

      test_formatter f;
      sel->write_nodes(f);
      REQUIRE(f.m_nodes.size() == 1); // number of nodes (1) written for moderator from changeset 2

      REQUIRE(
        test_formatter::node_t(
          element_info(1, 2, 2, "2017-03-19T20:18:00Z", 1, std::string("user_1"), true),
          0.0, 0.0,
          tags_t()) == f.m_nodes.front()); // redacted node 1 in changeset 2
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


