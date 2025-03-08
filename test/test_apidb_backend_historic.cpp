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

void init_user_changeset(test_database &tdb) {

  tdb.run_sql(R"(

    INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public)
    VALUES (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', TRUE);

    INSERT INTO changesets (id, user_id, created_at, closed_at)
    VALUES (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'),
           (3, 1, '2017-02-17T15:34:00Z', '2017-02-17T15:34:00Z');
  )");

}


TEST_CASE_METHOD(DatabaseTestsFixture, "test_historic_elements", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 2, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2);

      INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "timestamp", tile, version, redaction_id)
      VALUES (3, 0, 0, 2,  TRUE, '2015-03-02T18:27:00Z', 3221225472, 1, NULL),
             (3, 0, 0, 2, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2, NULL);
    )");
  }

  SECTION("Check reading and formatting two historical nodes with version number") {

    // check number of nodes selected
    CHECK(sel->select_historical_nodes( { { 3, 1 }, { 3, 2 } }) == 2);
    sel->write_nodes(f);

    // check number of nodes written
    CHECK(f.m_nodes.size() == 2);

    CHECK(
        test_formatter::node_t(
            element_info(3, 1, 2, "2015-03-02T18:27:00Z", 1,
                std::string("user_1"), true), 0.0, 0.0, tags_t())
            == f.m_nodes[0]); // first node written

    CHECK(
        test_formatter::node_t(
            element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1,
                std::string("user_1"), false), 0.0, 0.0, tags_t())
            == f.m_nodes[1]);  // second node written
  }

  SECTION("Check reading and formatting one current and one historic node") {

    // check number of historic nodes selected
    CHECK(sel->select_historical_nodes( { { 3, 2 } }) == 1);

    // check number of current nodes selected
    CHECK(sel->select_nodes( { 3 }) == 1);
    sel->write_nodes(f);

    // check number of nodes written, only one node found, v2
    CHECK(f.m_nodes.size() == 1);

    CHECK(
        test_formatter::node_t(
            element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1,
                std::string("user_1"), false), 0.0, 0.0, tags_t())
            == f.m_nodes[0]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_historic_dup_way", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 3, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2);

      INSERT INTO current_ways (id, changeset_id, "timestamp", visible, version)
      VALUES (1, 3, '2016-09-06T19:55:00Z', TRUE, 2);

      INSERT INTO current_way_nodes (way_id, node_id, sequence_id)
      VALUES (1, 3, 1);

      INSERT INTO ways (way_id, changeset_id, "timestamp", visible, version, redaction_id)
      VALUES (1, 3, '2016-09-06T19:55:00Z', TRUE, 2, NULL),
             (1, 3, '2016-09-06T19:54:00Z', TRUE, 1, NULL);

      INSERT INTO way_nodes (way_id, version, node_id, sequence_id)
      VALUES (1, 2, 3, 1),
             (1, 1, 3, 1),
             (1, 1, 2, 2);

    )");
  }

  SECTION("Check reading and formatting one current and one historic way") {

    // check number of historic ways selected
    CHECK(sel->select_historical_ways( { { 1, 2 } }) == 1);
    // check number of current ways selected
    CHECK(sel->select_ways( { 1 }) == 1);

    sel->write_ways(f);
    // check number of ways written
    CHECK(f.m_ways.size() == 1);

    CHECK(
        test_formatter::way_t(
            element_info(1, 2, 3, "2016-09-06T19:55:00Z", 1,
                std::string("user_1"), true), { 3 },        // nodes_t
            tags_t()) == f.m_ways[0]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_historic_dup_relation", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 3, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2);

      INSERT INTO current_relations (id, changeset_id, "timestamp", visible, version)
      VALUES (1, 3, '2016-09-19T18:49:00Z', TRUE, 2);

      INSERT INTO current_relation_members (relation_id, member_type, member_id, member_role, sequence_id)
      VALUES (1, 'Node', 3, 'foo', 1);

      INSERT INTO relations (relation_id, changeset_id, "timestamp", visible, version, redaction_id)
      VALUES (1, 3, '2016-09-19T18:49:00Z', TRUE, 2, NULL),
             (1, 3, '2016-09-19T18:48:00Z', TRUE, 1, NULL);

      INSERT INTO relation_members (relation_id, member_type, member_id, member_role, sequence_id, version)
      VALUES (1, 'Node', 3, 'foo', 1, 2),
             (1, 'Node', 3, 'bar', 1, 1);

    )");
  }

  SECTION("Check reading and formatting one current and one historic relation") {

    // check number of historic relations selected
    CHECK(sel->select_historical_relations( { { 1, 2 } }) == 1);
    // check number of current relations selected"
    CHECK(sel->select_relations( { 1 }) == 1);
    sel->write_relations(f);
    // check number of relations written
    CHECK(f.m_relations.size() == 1);

    members_t relation1_members { member_info(element_type::node, 3, "foo") };

    CHECK(
        test_formatter::relation_t(
            element_info(1, 2, 3, "2016-09-19T18:49:00Z", 1,
                std::string("user_1"), true), relation1_members, tags_t())
            == f.m_relations[0]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_node_history", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 2, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2);

      INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "timestamp", tile, version, redaction_id)
      VALUES (3, 0, 0, 2, TRUE, '2015-03-02T18:27:00Z', 3221225472, 1, NULL),
             (3, 0, 0, 2, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2, NULL);

      INSERT INTO node_tags(node_id, version, k, v)
      VALUES (3, 1, 'key1_1', 'value1'),
             (3, 1, 'key1_2', 'value2'),
             (3, 1, 'key1_3', 'value3'),
             (3, 2, 'key2_1', 'value4'),
             (3, 2, 'key2_2', 'value5');
    )");
  }

  SECTION("Two nodes with history") {

    // check number of node versions selected
    CHECK(sel->select_nodes_with_history({3}) == 2);
    sel->write_nodes(f);
    // ckeck number of nodes written
    CHECK(f.m_nodes.size() == 2);

    CHECK(
      test_formatter::node_t(
        element_info(3, 1, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), true),
        0.0, 0.0,
        tags_t( { { "key1_1", "value1" },
                  { "key1_2", "value2" },
                  { "key1_3", "value3" } })
        ) == f.m_nodes[0]);
    CHECK(
      test_formatter::node_t(
        element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
        0.0, 0.0,
        tags_t( { { "key2_1", "value4" },
                  { "key2_2", "value5" } })
        ) == f.m_nodes[1]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_way_history", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 3, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2);

      INSERT INTO current_ways (id, changeset_id, "timestamp", visible, version)
      VALUES (1, 3, '2016-09-06T19:55:00Z', TRUE, 2);

      INSERT INTO current_way_nodes (way_id, node_id, sequence_id)
      VALUES (1, 3, 1);

      INSERT INTO ways (way_id, changeset_id, "timestamp", visible, version, redaction_id)
      VALUES (1, 3, '2016-09-06T19:55:00Z', TRUE, 2, NULL),
             (1, 3, '2016-09-06T19:54:00Z', TRUE, 1, NULL);

      INSERT INTO way_nodes (way_id, version, node_id, sequence_id)
      VALUES (1, 2, 3, 1),
             (1, 1, 3, 1),
             (1, 1, 2, 2);

      INSERT INTO way_tags(way_id, version, k, v)
      VALUES (1, 1, 'key1_1', 'value1'),
             (1, 1, 'key1_2', 'value2'),
             (1, 1, 'key1_3', 'value3'),
             (1, 2, 'key2_1', 'value4'),
             (1, 2, 'key2_2', 'value5');

    )");
  }

  SECTION("One way with history") {

    // check number of way versions selected
    CHECK(sel->select_ways_with_history({1}) == 2);
    sel->write_ways(f);
    // check number of ways written
    CHECK(f.m_ways.size() == 2);

    CHECK(
      test_formatter::way_t(
        element_info(1, 1, 3, "2016-09-06T19:54:00Z", 1, std::string("user_1"), true),
        {3, 2},
        tags_t( { { "key1_1", "value1" },
                  { "key1_2", "value2" },
                  { "key1_3", "value3" } })
        ) == f.m_ways[0]);

    CHECK(
      test_formatter::way_t(
        element_info(1, 2, 3, "2016-09-06T19:55:00Z", 1, std::string("user_1"), true),
        {3},
        tags_t( { { "key2_1", "value4" },
                  { "key2_2", "value5" } })
        ) == f.m_ways[1]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_relation_history", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 3, FALSE, '2015-03-02T18:27:00Z', 3221225472, 2);

      INSERT INTO current_relations (id, changeset_id, "timestamp", visible, version)
      VALUES (1, 3, '2016-09-19T18:49:00Z', TRUE, 2);

      INSERT INTO current_relation_members (relation_id, member_type, member_id, member_role, sequence_id)
      VALUES (1, 'Node', 3, 'foo', 1);

      INSERT INTO relations (relation_id, changeset_id, "timestamp", visible, version, redaction_id)
      VALUES (1, 3, '2016-09-19T18:49:00Z', TRUE, 2, NULL),
             (1, 3, '2016-09-19T18:48:00Z', TRUE, 1, NULL);

      INSERT INTO relation_members (relation_id, member_type, member_id, member_role, sequence_id, version)
      VALUES (1, 'Node', 3, 'foo', 1, 2),
             (1, 'Node', 3, 'bar', 1, 1);

      INSERT INTO relation_tags(relation_id, version, k, v)
      VALUES (1, 1, 'key1_1', 'value1'),
             (1, 1, 'key1_2', 'value2'),
             (1, 1, 'key1_3', 'value3'),
             (1, 2, 'key2_1', 'value4'),
             (1, 2, 'key2_2', 'value5');
    )");
  }

  SECTION("One relation with history") {

    // check number of relation versions selected
    CHECK(sel->select_relations_with_history({1}) == 2);
    sel->write_relations(f);
    // check number of relations written"
    CHECK(f.m_relations.size() == 2);

    members_t relation1v1_members{member_info(element_type::node, 3, "bar")};
    members_t relation1v2_members{member_info(element_type::node, 3, "foo")};

    CHECK(
      test_formatter::relation_t(
        element_info(1, 1, 3, "2016-09-19T18:48:00Z", 1, std::string("user_1"), true),
        relation1v1_members,
        tags_t( { { "key1_1", "value1" },
                  { "key1_2", "value2" },
                  { "key1_3", "value3" } })
        ) == f.m_relations[0]);

    CHECK(
      test_formatter::relation_t(
        element_info(1, 2, 3, "2016-09-19T18:49:00Z", 1, std::string("user_1"), true),
        relation1v2_members,
        tags_t( { { "key2_1", "value4" },
                  { "key2_2", "value5" } })
        ) == f.m_relations[1]);
    }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_node_with_history_redacted", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

        INSERT INTO redactions (id, title, description, created_at, updated_at, user_id)
        VALUES (1, 'test redaction', 'test redaction description', '2017-02-04T16:56:00Z', '2017-02-04T16:56:00Z', 1);

        INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
        VALUES (3, 0, 0, 2, TRUE, '2017-02-04T16:57:00Z', 3221225472, 2);

        INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "timestamp", tile, version, redaction_id)
        VALUES (3, 0, 0, 2, TRUE, '2017-02-04T16:56:00Z', 3221225472, 1, 1),
               (3, 0, 0, 2, TRUE, '2017-02-04T16:57:00Z', 3221225472, 2, NULL);
    )");
  }

  SECTION("Redacted node with history as normal user and moderator") {
    // as a normal user, the redactions should not be visible

    // check number of node versions selected for regular user
    CHECK(sel->select_nodes_with_history( { 3 }) == 1);

    test_formatter f1;
    sel->write_nodes(f1);
    // check number of nodes written for regular user
    CHECK(f1.m_nodes.size() == 1);

    CHECK(
        test_formatter::node_t(
            element_info(3, 2, 2, "2017-02-04T16:57:00Z", 1,
                std::string("user_1"), true), 0.0, 0.0, tags_t())
            == f1.m_nodes[0]);

    // as a moderator, should have all redacted elements shown.
    // NOTE: the node versions which have already been selected are still selected.

    sel->set_redactions_visible(true);
    // check number of extra node versions selected for moderator
    CHECK(sel->select_nodes_with_history( { 3 }) == 1);

    test_formatter f2;
    sel->write_nodes(f2);
    // check number of nodes written for moderator
    CHECK(f2.m_nodes.size() == 2);

    CHECK(
        test_formatter::node_t(
            element_info(3, 1, 2, "2017-02-04T16:56:00Z", 1,
                std::string("user_1"), true), 0.0, 0.0, tags_t())
            == f2.m_nodes[0]);
    CHECK(
        test_formatter::node_t(
            element_info(3, 2, 2, "2017-02-04T16:57:00Z", 1,
                std::string("user_1"), true), 0.0, 0.0, tags_t())
            == f2.m_nodes[1]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_historical_nodes_redacted", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO redactions (id, title, description, created_at, updated_at, user_id)
      VALUES (1, 'test redaction', 'test redaction description', '2017-02-04T16:56:00Z', '2017-02-04T16:56:00Z', 1);

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 2, TRUE, '2017-02-04T16:57:00Z', 3221225472, 2);

      INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "timestamp", tile, version, redaction_id)
      VALUES (3, 0, 0, 2, TRUE, '2017-02-04T16:56:00Z', 3221225472, 1, 1),
             (3, 0, 0, 2, TRUE, '2017-02-04T16:57:00Z', 3221225472, 2, NULL);

   )");

  }

  SECTION("Redacted node version as normal user and moderator") {
    // as a normal user, the redactions should not be visible
    // check number of node versions selected for regular user
    CHECK(sel->select_historical_nodes( { { 3, 1 } }) == 0);

    test_formatter f1;
    sel->write_nodes(f1);
    // check number of nodes written for regular user
    CHECK(f1.m_nodes.size() == 0);

    // as a moderator, should have all redacted elements shown.
    // NOTE: the node versions which have already been selected are still selected.

    sel->set_redactions_visible(true);
    // check number of extra node versions selected for moderator
    CHECK(sel->select_historical_nodes( { { 3, 1 } }) == 1);

    test_formatter f2;
    sel->write_nodes(f2);
    // check number of nodes written for moderator
    CHECK(f2.m_nodes.size() == 1);

    CHECK(
        test_formatter::node_t(
            element_info(3, 1, 2, "2017-02-04T16:56:00Z", 1,
                std::string("user_1"), true), 0.0, 0.0, tags_t())
            == f2.m_nodes[0]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_way_with_history_redacted", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO redactions (id, title, description, created_at, updated_at, user_id)
      VALUES (1, 'test redaction', 'test redaction description', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1);

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 3, FALSE, '2017-02-17T18:27:00Z', 3221225472, 2);

      INSERT INTO current_ways (id, changeset_id, "timestamp", visible, version)
      VALUES (1, 3, '2017-02-17T19:55:00Z', TRUE, 2);

      INSERT INTO current_way_nodes (way_id, node_id, sequence_id)
      VALUES (1, 3, 1);

      INSERT INTO ways (way_id, changeset_id, "timestamp", visible, version, redaction_id)
      VALUES (1, 3, '2017-02-17T19:55:00Z', TRUE, 2, NULL),
             (1, 3, '2017-02-17T19:54:00Z', TRUE, 1, 1);

      INSERT INTO way_nodes (way_id, version, node_id, sequence_id)
      VALUES (1, 2, 3, 1),
             (1, 1, 3, 1),
             (1, 1, 2, 2);
    )");
  }

  // as a normal user, the redactions should not be visible
  SECTION("Way history with redacted version, as normal user and moderator") {
    // check number of way versions selected for regular user
    CHECK(sel->select_ways_with_history( { 1 }) == 1);

    test_formatter f1;
    sel->write_ways(f1);
    // check number of ways written for regular user
    CHECK(f1.m_ways.size() == 1);

    CHECK(
        test_formatter::way_t(
            element_info(1, 2, 3, "2017-02-17T19:55:00Z", 1,
                std::string("user_1"), true), { 3 }, tags_t()) == f1.m_ways[0]);

    // as a moderator (and setting the request flag), all the versions should be visible.
    sel->set_redactions_visible(true);
    // check number of extra way versions selected for moderator
    CHECK(sel->select_ways_with_history( { 1 }) == 1); // note: one is already selected

    test_formatter f2;
    sel->write_ways(f2);
    // check number of ways written for moderator
    CHECK(f2.m_ways.size() == 2);

    CHECK(
        test_formatter::way_t(
            element_info(1, 1, 3, "2017-02-17T19:54:00Z", 1,
                std::string("user_1"), true), { 3, 2 }, tags_t())
            == f2.m_ways[0]);
    CHECK(
        test_formatter::way_t(
            element_info(1, 2, 3, "2017-02-17T19:55:00Z", 1,
                std::string("user_1"), true), { 3 }, tags_t()) == f2.m_ways[1]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_historical_ways_redacted", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO redactions (id, title, description, created_at, updated_at, user_id)
      VALUES (1, 'test redaction', 'test redaction description', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1);

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 3, FALSE, '2017-02-17T18:27:00Z', 3221225472, 2);

      INSERT INTO current_ways (id, changeset_id, "timestamp", visible, version)
      VALUES (1, 3, '2017-02-17T19:55:00Z', TRUE, 2);

      INSERT INTO current_way_nodes (way_id, node_id, sequence_id)
      VALUES (1, 3, 1);

      INSERT INTO ways (way_id, changeset_id, "timestamp", visible, version, redaction_id)
      VALUES (1, 3, '2017-02-17T19:55:00Z', TRUE, 2, NULL),
             (1, 3, '2017-02-17T19:54:00Z', TRUE, 1, 1);

      INSERT INTO way_nodes (way_id, version, node_id, sequence_id)
      VALUES (1, 2, 3, 1),
             (1, 1, 3, 1),
             (1, 1, 2, 2);
    )");
  }

  SECTION("As normal user, the redactions should not be visible") {
    // check number of way versions selected for regular user
    CHECK(sel->select_historical_ways( { { 1, 1 } }) == 0);
    sel->write_ways(f);
    // check number of ways written for regular user
    CHECK(f.m_ways.size() == 0);
  }

  // as a moderator (and setting the request flag), all the versions should be visible.

  SECTION("As moderator, the redactions should not be visible") {
    sel->set_redactions_visible(true);
    // check number of way versions selected for moderator
    CHECK(sel->select_historical_ways( { { 1, 1 } }) == 1);

    sel->write_ways(f);
    CHECK(f.m_ways.size() == 1);

    CHECK(
        test_formatter::way_t(
            element_info(1, 1, 3, "2017-02-17T19:54:00Z", 1,
                std::string("user_1"), true), { 3, 2 }, tags_t())
            == f.m_ways[0]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_relation_with_history_redacted", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

      INSERT INTO redactions (id, title, description, created_at, updated_at, user_id)
      VALUES (1, 'test redaction', 'test redaction description', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1);

      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      VALUES (3, 0, 0, 3, FALSE, '2017-02-17T15:34:00Z', 3221225472, 2);

      INSERT INTO current_relations (id, changeset_id, "timestamp", visible, version)
      VALUES (1, 3, '2017-02-17T15:34:00Z', TRUE, 2);

      INSERT INTO current_relation_members (relation_id, member_type, member_id, member_role, sequence_id)
      VALUES (1, 'Node', 3, 'foo', 1);

      INSERT INTO relations (relation_id, changeset_id, "timestamp", visible, version, redaction_id)
      VALUES (1, 3, '2017-02-17T15:34:00Z', TRUE, 2, NULL),
             (1, 3, '2017-02-17T15:34:00Z', TRUE, 1, 1);

      INSERT INTO relation_members (relation_id, member_type, member_id, member_role, sequence_id, version)
      VALUES (1, 'Node', 3, 'foo', 1, 2),
             (1, 'Node', 3, 'bar', 1, 1);
    )");
  }
  // as a normal user, the redactions should not be visible
  SECTION("Relation with redaction as normal user and moderator")
  {
    {
      // check number of relation versions selected for regular user
      CHECK(sel->select_relations_with_history( { 1 }) == 1);
      test_formatter f;
      sel->write_relations(f);
      // check number of relations written for regular user
      CHECK(f.m_relations.size() == 1);

      members_t relation1v2_members;
      relation1v2_members.push_back(member_info(element_type::node, 3, "foo"));

      CHECK(
          test_formatter::relation_t(
              element_info(1, 2, 3, "2017-02-17T15:34:00Z", 1,
                  std::string("user_1"), true), relation1v2_members, tags_t())
              == f.m_relations[0]);
    }

    // as a moderator (and setting the request flag), all the versions should be visible.
    {
      sel->set_redactions_visible(true);
      // check number of extra relation versions selected for moderator
      CHECK(sel->select_relations_with_history( { 1 }) == 1); // note: one is already selected

      test_formatter f;
      sel->write_relations(f);
      // check number of relations written for moderator
      CHECK(f.m_relations.size() == 2);

      const members_t relation1v1_members { member_info(element_type::node, 3, "bar") };
      const members_t relation1v2_members { member_info(element_type::node, 3, "foo") };

      CHECK(
          test_formatter::relation_t(
              element_info(1, 1, 3, "2017-02-17T15:34:00Z", 1,
                  std::string("user_1"), true), relation1v1_members, tags_t())
              == f.m_relations[0]);

      CHECK(
          test_formatter::relation_t(
              element_info(1, 2, 3, "2017-02-17T15:34:00Z", 1,
                  std::string("user_1"), true), relation1v2_members, tags_t())
              == f.m_relations[1]);
    }
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_historical_relations_redacted", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

        INSERT INTO redactions (id, title, description, created_at, updated_at, user_id)
        VALUES (1, 'test redaction', 'test redaction description', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1);

        INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
        VALUES (3, 0, 0, 3, FALSE, '2017-02-17T15:34:00Z', 3221225472, 2);

        INSERT INTO current_relations (id, changeset_id, "timestamp", visible, version)
        VALUES (1, 3, '2017-02-17T15:34:00Z', TRUE, 2);

        INSERT INTO current_relation_members (relation_id, member_type, member_id, member_role, sequence_id)
        VALUES (1, 'Node', 3, 'foo', 1);

        INSERT INTO relations (relation_id, changeset_id, "timestamp", visible, version, redaction_id)
        VALUES (1, 3, '2017-02-17T15:34:00Z', TRUE, 2, NULL),
               (1, 3, '2017-02-17T15:34:00Z', TRUE, 1, 1);

        INSERT INTO relation_members (relation_id, member_type, member_id, member_role, sequence_id, version)
        VALUES (1, 'Node', 3, 'foo', 1, 2),
               (1, 'Node', 3, 'bar', 1, 1);

    )");
  }

  // as a normal user, the redactions should not be visible
  SECTION("Relation redacted version should not be visible as normal user") {
    // check number of relation versions selected for regular user
    CHECK(sel->select_historical_relations({{1,1}}) == 0);
    sel->write_relations(f);
    // check number of relations written for regular user
    CHECK(f.m_relations.size() == 0);
  }

  // as a moderator (and setting the request flag), all the versions should be visible.
  SECTION("Relation redacted version is visible for moderator"){
    sel->set_redactions_visible(true);
    // check number of relation versions selected for moderator
    CHECK(sel->select_historical_relations( { { 1, 1 } }) == 1);

    sel->write_relations(f);
    // check number of relations written for moderator
    CHECK(f.m_relations.size() == 1);

    members_t relation1v1_members { member_info(element_type::node, 3, "bar") };

    CHECK(
        test_formatter::relation_t(
            element_info(1, 1, 3, "2017-02-17T15:34:00Z", 1,
                std::string("user_1"), true), relation1v1_members, tags_t())
            == f.m_relations[0]);
  }
}

TEST_CASE_METHOD(DatabaseTestsFixture, "test_historic_way_node_order", "[historic][db]" ) {

  auto sel = tdb.get_data_selection();
  test_formatter f;

  SECTION("Initialize test data") {

    init_user_changeset(tdb);
    tdb.run_sql(R"(

        INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
        VALUES (3,   0,  0, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (4,  10, 10, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (5,  20, 20, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (6,  30, 30, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (7,  40, 40, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (8,  50, 50, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (9,  60, 60, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (10, 70, 70, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2);

        INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
        VALUES (3,   0,  0, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (4,  10, 10, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (5,  20, 20, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (6,  30, 30, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (7,  40, 40, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (8,  50, 50, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (9,  60, 60, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2),
               (10, 70, 70, 3, FALSE, '2017-02-27T19:33:00Z', 3221225472, 2);

        INSERT INTO current_ways (id, changeset_id, "timestamp", visible, version)
        VALUES (1, 3, '2017-02-27T19:33:00Z', TRUE, 2);

        INSERT INTO current_way_nodes (way_id, node_id, sequence_id)
        VALUES (1, 3, 1),
               (1, 4, 2),
               (1, 5, 3),
               (1, 6, 4),
               (1, 7, 5),
               (1, 8, 6),
               (1, 9, 7),
               (1, 10, 8);

        INSERT INTO ways (way_id, changeset_id, "timestamp", visible, version, redaction_id)
        VALUES (1, 3, '2017-02-27T19:33:00Z', TRUE, 2, NULL),
               (1, 3, '2017-02-27T19:33:00Z', TRUE, 1, NULL);

        INSERT INTO way_nodes (way_id, version, node_id, sequence_id)
        VALUES (1, 1, 3, 8),
               (1, 1, 4, 7),
               (1, 1, 5, 6),
               (1, 1, 6, 5),
               (1, 1, 7, 4),
               (1, 1, 8, 3),
               (1, 1, 9, 2),
               (1, 1, 10, 1),
               (1, 2, 3, 1),
               (1, 2, 4, 2),
               (1, 2, 5, 3),
               (1, 2, 6, 4),
               (1, 2, 7, 5),
               (1, 2, 8, 6),
               (1, 2, 9, 7),
               (1, 2, 10, 8);

    )");
  }

  SECTION("Check correct way history sequence") {

    // check number of way versions with history selected
    CHECK(sel->select_ways_with_history( { 1 }) == 2);
    sel->write_ways(f);
    // check number of ways written
    CHECK(f.m_ways.size() == 2);

    const nodes_t way_v1_nds( { 10, 9, 8, 7, 6, 5, 4, 3 });
    const nodes_t way_v2_nds( { 3, 4, 5, 6, 7, 8, 9, 10 });

    CHECK(
        test_formatter::way_t(
            element_info(1, 1, 3, "2017-02-27T19:33:00Z", 1,
                std::string("user_1"), true), way_v1_nds, tags_t())
            == f.m_ways[0]);

    CHECK(
        test_formatter::way_t(
            element_info(1, 2, 3, "2017-02-27T19:33:00Z", 1,
                std::string("user_1"), true), way_v2_nds, tags_t())
            == f.m_ways[1]);
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
