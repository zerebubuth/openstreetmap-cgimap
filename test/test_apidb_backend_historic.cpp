#include <iostream>
#include <stdexcept>
#include <boost/noncopyable.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>

#include <sys/time.h>
#include <stdio.h>

#include "cgimap/config.hpp"
#include "cgimap/time.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"
#include "test_request.hpp"

namespace {

template <typename T>
void assert_equal(const T& a, const T&b, const std::string &message) {
  if (a != b) {
    throw std::runtime_error(
      (boost::format(
        "Expecting %1% to be equal, but %2% != %3%")
       % message % a % b).str());
  }
}

void test_historic_elements(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 2, false, '2015-03-02T18:27:00Z', 3221225472, 2); "

    "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
    "                   \"timestamp\", tile, version, redaction_id) "
    "VALUES "
    "  (3, 0, 0, 2, true,  '2015-03-02T18:27:00Z', 3221225472, 1, NULL), "
    "  (3, 0, 0, 2, false, '2015-03-02T18:27:00Z', 3221225472, 2, NULL); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_edition_t> editions;
  editions.push_back(std::make_pair(osm_nwr_id_t(3), osm_version_t(1)));
  editions.push_back(std::make_pair(osm_nwr_id_t(3), osm_version_t(2)));

  assert_equal<int>(
    sel->select_historical_nodes(editions), 2,
    "number of nodes selected");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 2, "number of nodes written");

  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 1, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[1], "second node written");
}

void test_historic_dup(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 2, false, '2015-03-02T18:27:00Z', 3221225472, 2); "

    "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
    "                   \"timestamp\", tile, version, redaction_id) "
    "VALUES "
    "  (3, 0, 0, 2, true,  '2015-03-02T18:27:00Z', 3221225472, 1, NULL), "
    "  (3, 0, 0, 2, false, '2015-03-02T18:27:00Z', 3221225472, 2, NULL); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_edition_t> editions;
  editions.push_back(std::make_pair(osm_nwr_id_t(3), osm_version_t(2)));
  assert_equal<int>(
    sel->select_historical_nodes(editions), 1,
    "number of historic nodes selected");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(3);
  assert_equal<int>(
    sel->select_nodes(ids), 1,
    "number of current nodes selected");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 1, "number of nodes written");

  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "node written");
}

void test_historic_dup_way(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2015-03-02T18:27:00Z', 3221225472, 2); "

    "INSERT INTO current_ways (id, changeset_id, \"timestamp\", visible, version) "
    "VALUES "
    "  (1, 3, '2016-09-06T19:55:00Z', true, 2); "

    "INSERT INTO current_way_nodes (way_id, node_id, sequence_id) "
    "VALUES "
    "  (1, 3, 1); "

    "INSERT INTO ways (way_id, changeset_id, \"timestamp\", visible, "
    "                  version, redaction_id) "
    "VALUES "
    "  (1, 3, '2016-09-06T19:55:00Z', true, 2, NULL), "
    "  (1, 3, '2016-09-06T19:54:00Z', true, 1, NULL); "

    "INSERT INTO way_nodes (way_id, version, node_id, sequence_id) "
    "VALUES "
    "  (1, 2, 3, 1), "
    "  (1, 1, 3, 1), "
    "  (1, 1, 2, 2); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_edition_t> editions;
  editions.push_back(std::make_pair(osm_nwr_id_t(1), osm_version_t(2)));
  assert_equal<int>(
    sel->select_historical_ways(editions), 1,
    "number of historic ways selected");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  assert_equal<int>(
    sel->select_ways(ids), 1,
    "number of current ways selected");

  test_formatter f;
  sel->write_ways(f);
  assert_equal<size_t>(f.m_ways.size(), 1, "number of ways written");

  nodes_t way1_nds;
  way1_nds.push_back(3);

  assert_equal<test_formatter::way_t>(
    test_formatter::way_t(
      element_info(1, 2, 3, "2016-09-06T19:55:00Z", 1, std::string("user_1"), true),
      way1_nds,
      tags_t()
      ),
    f.m_ways[0], "way written");
}

void test_historic_dup_relation(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2015-03-02T18:27:00Z', 3221225472, 2); "

    "INSERT INTO current_relations (id, changeset_id, \"timestamp\", "
    "                               visible, version) "
    "VALUES "
    "  (1, 3, '2016-09-19T18:49:00Z', true, 2); "

    "INSERT INTO current_relation_members ("
        "relation_id, member_type, member_id, member_role, sequence_id) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1); "

    "INSERT INTO relations (relation_id, changeset_id, \"timestamp\", "
    "                       visible, version, redaction_id) "
    "VALUES "
    "  (1, 3, '2016-09-19T18:49:00Z', true, 2, NULL), "
    "  (1, 3, '2016-09-19T18:48:00Z', true, 1, NULL); "

    "INSERT INTO relation_members (relation_id, member_type, member_id, "
    "                              member_role, sequence_id, version) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1, 2), "
    "  (1, 'Node', 3, 'bar', 1, 1); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_edition_t> editions;
  editions.push_back(std::make_pair(osm_nwr_id_t(1), osm_version_t(2)));
  assert_equal<int>(
    sel->select_historical_relations(editions), 1,
    "number of historic relations selected");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  assert_equal<int>(
    sel->select_relations(ids), 1,
    "number of current relations selected");

  test_formatter f;
  sel->write_relations(f);
  assert_equal<size_t>(f.m_relations.size(), 1, "number of relations written");

  members_t relation1_members;
  relation1_members.push_back(member_info(element_type_node, 3, "foo"));

  assert_equal<test_formatter::relation_t>(
    test_formatter::relation_t(
      element_info(1, 2, 3, "2016-09-19T18:49:00Z", 1, std::string("user_1"), true),
      relation1_members,
      tags_t()
      ),
    f.m_relations[0], "relation written");
}

void test_node_history(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 2, false, '2015-03-02T18:27:00Z', 3221225472, 2); "

    "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
    "                   \"timestamp\", tile, version, redaction_id) "
    "VALUES "
    "  (3, 0, 0, 2, true,  '2015-03-02T18:27:00Z', 3221225472, 1, NULL), "
    "  (3, 0, 0, 2, false, '2015-03-02T18:27:00Z', 3221225472, 2, NULL); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(3);

  assert_equal<int>(
    sel->select_nodes_with_history(ids), 2,
    "number of node versions selected");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 2, "number of nodes written");

  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 1, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[1], "second node written");
}

void test_way_history(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2015-03-02T18:27:00Z', 3221225472, 2); "

    "INSERT INTO current_ways (id, changeset_id, \"timestamp\", visible, version) "
    "VALUES "
    "  (1, 3, '2016-09-06T19:55:00Z', true, 2); "

    "INSERT INTO current_way_nodes (way_id, node_id, sequence_id) "
    "VALUES "
    "  (1, 3, 1); "

    "INSERT INTO ways (way_id, changeset_id, \"timestamp\", visible, "
    "                  version, redaction_id) "
    "VALUES "
    "  (1, 3, '2016-09-06T19:55:00Z', true, 2, NULL), "
    "  (1, 3, '2016-09-06T19:54:00Z', true, 1, NULL); "

    "INSERT INTO way_nodes (way_id, version, node_id, sequence_id) "
    "VALUES "
    "  (1, 2, 3, 1), "
    "  (1, 1, 3, 1), "
    "  (1, 1, 2, 2); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);

  assert_equal<int>(
    sel->select_ways_with_history(ids), 2,
    "number of way versions selected");

  test_formatter f;
  sel->write_ways(f);
  assert_equal<size_t>(f.m_ways.size(), 2, "number of ways written");

  assert_equal<test_formatter::way_t>(
    test_formatter::way_t(
      element_info(1, 1, 3, "2016-09-06T19:54:00Z", 1, std::string("user_1"), true),
      {3, 2},
      tags_t()
      ),
    f.m_ways[0], "first way written");
  assert_equal<test_formatter::way_t>(
    test_formatter::way_t(
      element_info(1, 2, 3, "2016-09-06T19:55:00Z", 1, std::string("user_1"), true),
      {3},
      tags_t()
      ),
    f.m_ways[1], "second way written");
}

void test_relation_history(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2015-03-02T18:27:00Z', 3221225472, 2); "

    "INSERT INTO current_relations (id, changeset_id, \"timestamp\", "
    "                               visible, version) "
    "VALUES "
    "  (1, 3, '2016-09-19T18:49:00Z', true, 2); "

    "INSERT INTO current_relation_members ("
        "relation_id, member_type, member_id, member_role, sequence_id) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1); "

    "INSERT INTO relations (relation_id, changeset_id, \"timestamp\", "
    "                       visible, version, redaction_id) "
    "VALUES "
    "  (1, 3, '2016-09-19T18:49:00Z', true, 2, NULL), "
    "  (1, 3, '2016-09-19T18:48:00Z', true, 1, NULL); "

    "INSERT INTO relation_members (relation_id, member_type, member_id, "
    "                              member_role, sequence_id, version) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1, 2), "
    "  (1, 'Node', 3, 'bar', 1, 1); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);

  assert_equal<int>(
    sel->select_relations_with_history(ids), 2,
    "number of relation versions selected");

  test_formatter f;
  sel->write_relations(f);
  assert_equal<size_t>(f.m_relations.size(), 2, "number of relations written");

  members_t relation1v1_members, relation1v2_members;
  relation1v1_members.push_back(member_info(element_type_node, 3, "bar"));
  relation1v2_members.push_back(member_info(element_type_node, 3, "foo"));

  assert_equal<test_formatter::relation_t>(
    test_formatter::relation_t(
      element_info(1, 1, 3, "2016-09-19T18:48:00Z", 1, std::string("user_1"), true),
      relation1v1_members,
      tags_t()
      ),
    f.m_relations[0], "first relation written");
  assert_equal<test_formatter::relation_t>(
    test_formatter::relation_t(
      element_info(1, 2, 3, "2016-09-19T18:49:00Z", 1, std::string("user_1"), true),
      relation1v2_members,
      tags_t()
      ),
    f.m_relations[1], "second relation written");
}

void test_node_with_history_redacted(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO redactions (id, title, created_at, updated_at, user_id) "
    "VALUES "
    "  (1, 'test redaction', '2017-02-04T16:56:00Z', '2017-02-04T16:56:00Z', 1); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 2, true, '2017-02-04T16:57:00Z', 3221225472, 2); "

    "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
    "                   \"timestamp\", tile, version, redaction_id) "
    "VALUES "
    "  (3, 0, 0, 2, true, '2017-02-04T16:56:00Z', 3221225472, 1, 1), "
    "  (3, 0, 0, 2, true, '2017-02-04T16:57:00Z', 3221225472, 2, NULL); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  // as a normal user, the redactions should not be visible
  {
    std::vector<osm_nwr_id_t> ids;
    ids.push_back(3);

    assert_equal<int>(sel->select_nodes_with_history(ids), 1,
      "number of node versions selected for regular user");

    test_formatter f;
    sel->write_nodes(f);
    assert_equal<size_t>(f.m_nodes.size(), 1,
      "number of nodes written for regular user");

    assert_equal<test_formatter::node_t>(
      test_formatter::node_t(
        element_info(3, 2, 2, "2017-02-04T16:57:00Z", 1, std::string("user_1"), true),
        0.0, 0.0,
        tags_t()
        ),
      f.m_nodes[0], "node written");
  }

  // as a moderator, should have all redacted elements shown.
  // NOTE: the node versions which have already been selected are still
  // selected.
  sel->set_redactions_visible(true);
  {
    std::vector<osm_nwr_id_t> ids;
    ids.push_back(3);

    assert_equal<int>(sel->select_nodes_with_history(ids), 1,
      "number of extra node versions selected for moderator");

    test_formatter f;
    sel->write_nodes(f);
    assert_equal<size_t>(f.m_nodes.size(), 2,
      "number of nodes written for moderator");

    assert_equal<test_formatter::node_t>(
      test_formatter::node_t(
        element_info(3, 1, 2, "2017-02-04T16:56:00Z", 1, std::string("user_1"), true),
        0.0, 0.0,
        tags_t()
        ),
      f.m_nodes[0], "first node written");
    assert_equal<test_formatter::node_t>(
      test_formatter::node_t(
        element_info(3, 2, 2, "2017-02-04T16:57:00Z", 1, std::string("user_1"), true),
        0.0, 0.0,
        tags_t()
        ),
      f.m_nodes[1], "second node written");
  }
}

void test_historical_nodes_redacted(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (2, 1, '2013-11-14T02:10:00Z', '2013-11-14T03:10:00Z'); "

    "INSERT INTO redactions (id, title, created_at, updated_at, user_id) "
    "VALUES "
    "  (1, 'test redaction', '2017-02-04T16:56:00Z', '2017-02-04T16:56:00Z', 1); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 2, true, '2017-02-04T16:57:00Z', 3221225472, 2); "

    "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "
    "                   \"timestamp\", tile, version, redaction_id) "
    "VALUES "
    "  (3, 0, 0, 2, true, '2017-02-04T16:56:00Z', 3221225472, 1, 1), "
    "  (3, 0, 0, 2, true, '2017-02-04T16:57:00Z', 3221225472, 2, NULL); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  // as a normal user, the redactions should not be visible
  {
    std::vector<osm_edition_t> eds;
    eds.emplace_back(3,1);

    assert_equal<int>(sel->select_historical_nodes(eds), 0,
      "number of node versions selected for regular user");

    test_formatter f;
    sel->write_nodes(f);
    assert_equal<size_t>(f.m_nodes.size(), 0,
      "number of nodes written for regular user");
  }

  // as a moderator, should have all redacted elements shown.
  // NOTE: the node versions which have already been selected are still
  // selected.
  sel->set_redactions_visible(true);
  {
    std::vector<osm_edition_t> eds;
    eds.emplace_back(3,1);

    assert_equal<int>(sel->select_historical_nodes(eds), 1,
      "number of extra node versions selected for moderator");

    test_formatter f;
    sel->write_nodes(f);
    assert_equal<size_t>(f.m_nodes.size(), 1,
      "number of nodes written for moderator");

    assert_equal<test_formatter::node_t>(
      test_formatter::node_t(
        element_info(3, 1, 2, "2017-02-04T16:56:00Z", 1, std::string("user_1"), true),
        0.0, 0.0,
        tags_t()
        ),
      f.m_nodes[0], "node written");
  }
}

void test_way_with_history_redacted(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2017-02-17T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2017-02-17T02:10:00Z', '2017-02-17T03:10:00Z'); "

    "INSERT INTO redactions (id, title, created_at, updated_at, user_id) "
    "VALUES "
    "  (1, 'test redaction', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2017-02-17T18:27:00Z', 3221225472, 2); "

    "INSERT INTO current_ways (id, changeset_id, \"timestamp\", visible, version) "
    "VALUES "
    "  (1, 3, '2017-02-17T19:55:00Z', true, 2); "

    "INSERT INTO current_way_nodes (way_id, node_id, sequence_id) "
    "VALUES "
    "  (1, 3, 1); "

    "INSERT INTO ways (way_id, changeset_id, \"timestamp\", visible, "
    "                  version, redaction_id) "
    "VALUES "
    "  (1, 3, '2017-02-17T19:55:00Z', true, 2, NULL), "
    "  (1, 3, '2017-02-17T19:54:00Z', true, 1, 1); "

    "INSERT INTO way_nodes (way_id, version, node_id, sequence_id) "
    "VALUES "
    "  (1, 2, 3, 1), "
    "  (1, 1, 3, 1), "
    "  (1, 1, 2, 2); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  // as a normal user, the redactions should not be visible
  {
    std::vector<osm_nwr_id_t> ids;
    ids.push_back(1);

    assert_equal<int>(
      sel->select_ways_with_history(ids), 1,
      "number of way versions selected for regular user");

    test_formatter f;
    sel->write_ways(f);
    assert_equal<size_t>(f.m_ways.size(), 1, "number of ways written for regular user");

    assert_equal<test_formatter::way_t>(
      test_formatter::way_t(
        element_info(1, 2, 3, "2017-02-17T19:55:00Z", 1, std::string("user_1"), true),
        {3},
        tags_t()
        ),
      f.m_ways[0], "way for regular user");
  }

  // as a moderator (and setting the request flag), all the versions should be
  // visible.
  sel->set_redactions_visible(true);
  {
    std::vector<osm_nwr_id_t> ids;
    ids.push_back(1);

    assert_equal<int>(
      sel->select_ways_with_history(ids), 1, // note: one is already selected
      "number of extra way versions selected for moderator");

    test_formatter f;
    sel->write_ways(f);
    assert_equal<size_t>(f.m_ways.size(), 2, "number of ways written for moderator");

    assert_equal<test_formatter::way_t>(
      test_formatter::way_t(
        element_info(1, 1, 3, "2017-02-17T19:54:00Z", 1, std::string("user_1"), true),
        {3, 2},
        tags_t()
        ),
      f.m_ways[0], "first way written");
    assert_equal<test_formatter::way_t>(
      test_formatter::way_t(
        element_info(1, 2, 3, "2017-02-17T19:55:00Z", 1, std::string("user_1"), true),
        {3},
        tags_t()
        ),
      f.m_ways[1], "second way written");
  }
}

void test_historical_ways_redacted(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2017-02-17T02:10:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2017-02-17T02:10:00Z', '2017-02-17T03:10:00Z'); "

    "INSERT INTO redactions (id, title, created_at, updated_at, user_id) "
    "VALUES "
    "  (1, 'test redaction', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2017-02-17T18:27:00Z', 3221225472, 2); "

    "INSERT INTO current_ways (id, changeset_id, \"timestamp\", visible, version) "
    "VALUES "
    "  (1, 3, '2017-02-17T19:55:00Z', true, 2); "

    "INSERT INTO current_way_nodes (way_id, node_id, sequence_id) "
    "VALUES "
    "  (1, 3, 1); "

    "INSERT INTO ways (way_id, changeset_id, \"timestamp\", visible, "
    "                  version, redaction_id) "
    "VALUES "
    "  (1, 3, '2017-02-17T19:55:00Z', true, 2, NULL), "
    "  (1, 3, '2017-02-17T19:54:00Z', true, 1, 1); "

    "INSERT INTO way_nodes (way_id, version, node_id, sequence_id) "
    "VALUES "
    "  (1, 2, 3, 1), "
    "  (1, 1, 3, 1), "
    "  (1, 1, 2, 2); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  // as a normal user, the redactions should not be visible
  {
    std::vector<osm_edition_t> eds;
    eds.emplace_back(1, 1);

    assert_equal<int>(
      sel->select_historical_ways(eds), 0,
      "number of way versions selected for regular user");

    test_formatter f;
    sel->write_ways(f);
    assert_equal<size_t>(f.m_ways.size(), 0, "number of ways written for regular user");
  }

  // as a moderator (and setting the request flag), all the versions should be
  // visible.
  sel->set_redactions_visible(true);
  {
    std::vector<osm_edition_t> eds;
    eds.emplace_back(1, 1);

    assert_equal<int>(
      sel->select_historical_ways(eds), 1,
      "number of way versions selected for moderator");

    test_formatter f;
    sel->write_ways(f);
    assert_equal<size_t>(f.m_ways.size(), 1, "number of ways written for moderator");

    assert_equal<test_formatter::way_t>(
      test_formatter::way_t(
        element_info(1, 1, 3, "2017-02-17T19:54:00Z", 1, std::string("user_1"), true),
        {3, 2},
        tags_t()
        ),
      f.m_ways[0], "way written");
  }
}

void test_relation_with_history_redacted(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2017-02-17T15:34:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2017-02-17T15:34:00Z', '2017-02-17T15:34:00Z'); "

    "INSERT INTO redactions (id, title, created_at, updated_at, user_id) "
    "VALUES "
    "  (1, 'test redaction', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2017-02-17T15:34:00Z', 3221225472, 2); "

    "INSERT INTO current_relations (id, changeset_id, \"timestamp\", "
    "                               visible, version) "
    "VALUES "
    "  (1, 3, '2017-02-17T15:34:00Z', true, 2); "

    "INSERT INTO current_relation_members ("
        "relation_id, member_type, member_id, member_role, sequence_id) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1); "

    "INSERT INTO relations (relation_id, changeset_id, \"timestamp\", "
    "                       visible, version, redaction_id) "
    "VALUES "
    "  (1, 3, '2017-02-17T15:34:00Z', true, 2, NULL), "
    "  (1, 3, '2017-02-17T15:34:00Z', true, 1, 1); "

    "INSERT INTO relation_members (relation_id, member_type, member_id, "
    "                              member_role, sequence_id, version) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1, 2), "
    "  (1, 'Node', 3, 'bar', 1, 1); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  // as a normal user, the redactions should not be visible
  {
    std::vector<osm_nwr_id_t> ids;
    ids.push_back(1);

    assert_equal<int>(
      sel->select_relations_with_history(ids), 1,
      "number of relation versions selected for regular user");

    test_formatter f;
    sel->write_relations(f);
    assert_equal<size_t>(f.m_relations.size(), 1, "number of relations written for regular user");

    members_t relation1v2_members;
    relation1v2_members.push_back(member_info(element_type_node, 3, "foo"));

    assert_equal<test_formatter::relation_t>(
      test_formatter::relation_t(
        element_info(1, 2, 3, "2017-02-17T15:34:00Z", 1, std::string("user_1"), true),
        relation1v2_members,
        tags_t()
        ),
      f.m_relations[0], "relation written");
  }

  // as a moderator (and setting the request flag), all the versions should be
  // visible.
  sel->set_redactions_visible(true);
  {
    std::vector<osm_nwr_id_t> ids;
    ids.push_back(1);

    assert_equal<int>(
      sel->select_relations_with_history(ids), 1, // note: one is already selected
      "number of extra relation versions selected for moderator");

    test_formatter f;
    sel->write_relations(f);
    assert_equal<size_t>(f.m_relations.size(), 2, "number of relations written for moderator");

    members_t relation1v1_members, relation1v2_members;
    relation1v1_members.push_back(member_info(element_type_node, 3, "bar"));
    relation1v2_members.push_back(member_info(element_type_node, 3, "foo"));

    assert_equal<test_formatter::relation_t>(
      test_formatter::relation_t(
        element_info(1, 1, 3, "2017-02-17T15:34:00Z", 1, std::string("user_1"), true),
        relation1v1_members,
        tags_t()
        ),
      f.m_relations[0], "first relation written");
    assert_equal<test_formatter::relation_t>(
      test_formatter::relation_t(
        element_info(1, 2, 3, "2017-02-17T15:34:00Z", 1, std::string("user_1"), true),
        relation1v2_members,
        tags_t()
        ),
      f.m_relations[1], "second relation written");
  }
}

void test_historical_relations_redacted(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2017-02-17T15:34:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2017-02-17T15:34:00Z', '2017-02-17T15:34:00Z'); "

    "INSERT INTO redactions (id, title, created_at, updated_at, user_id) "
    "VALUES "
    "  (1, 'test redaction', '2017-02-17T16:56:00Z', '2017-02-17T16:56:00Z', 1); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  (3, 0, 0, 3, false, '2017-02-17T15:34:00Z', 3221225472, 2); "

    "INSERT INTO current_relations (id, changeset_id, \"timestamp\", "
    "                               visible, version) "
    "VALUES "
    "  (1, 3, '2017-02-17T15:34:00Z', true, 2); "

    "INSERT INTO current_relation_members ("
        "relation_id, member_type, member_id, member_role, sequence_id) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1); "

    "INSERT INTO relations (relation_id, changeset_id, \"timestamp\", "
    "                       visible, version, redaction_id) "
    "VALUES "
    "  (1, 3, '2017-02-17T15:34:00Z', true, 2, NULL), "
    "  (1, 3, '2017-02-17T15:34:00Z', true, 1, 1); "

    "INSERT INTO relation_members (relation_id, member_type, member_id, "
    "                              member_role, sequence_id, version) "
    "VALUES "
    "  (1, 'Node', 3, 'foo', 1, 2), "
    "  (1, 'Node', 3, 'bar', 1, 1); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  // as a normal user, the redactions should not be visible
  {
    std::vector<osm_edition_t> eds;
    eds.emplace_back(1, 1);

    assert_equal<int>(
      sel->select_historical_relations(eds), 0,
      "number of relation versions selected for regular user");

    test_formatter f;
    sel->write_relations(f);
    assert_equal<size_t>(f.m_relations.size(), 0, "number of relations written for regular user");
  }

  // as a moderator (and setting the request flag), all the versions should be
  // visible.
  sel->set_redactions_visible(true);
  {
    std::vector<osm_edition_t> eds;
    eds.emplace_back(1, 1);

    assert_equal<int>(
      sel->select_historical_relations(eds), 1,
      "number of relation versions selected for moderator");

    test_formatter f;
    sel->write_relations(f);
    assert_equal<size_t>(f.m_relations.size(), 1, "number of relations written for moderator");

    members_t relation1v1_members;
    relation1v1_members.push_back(member_info(element_type_node, 3, "bar"));

    assert_equal<test_formatter::relation_t>(
      test_formatter::relation_t(
        element_info(1, 1, 3, "2017-02-17T15:34:00Z", 1, std::string("user_1"), true),
        relation1v1_members,
        tags_t()
        ),
      f.m_relations[0], "first relation written");
  }
}

void test_historic_way_node_order(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2017-02-27T19:33:00Z', 'user_1', true); "

    "INSERT INTO changesets (id, user_id, created_at, closed_at) "
    "VALUES "
    "  (3, 1, '2017-02-27T19:33:00Z', '2017-02-27T19:33:00Z'); "

    "INSERT INTO current_nodes (id, latitude, longitude, changeset_id, "
    "                           visible, \"timestamp\", tile, version) "
    "VALUES "
    "  ( 3,  0,  0, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 4, 10, 10, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 5, 20, 20, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 6, 30, 30, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 7, 40, 40, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 8, 50, 50, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 9, 60, 60, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  (10, 70, 70, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2); "

    "INSERT INTO nodes (node_id, latitude, longitude, changeset_id, "
    "                   visible, \"timestamp\", tile, version) "
    "VALUES "
    "  ( 3,  0,  0, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 4, 10, 10, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 5, 20, 20, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 6, 30, 30, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 7, 40, 40, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 8, 50, 50, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  ( 9, 60, 60, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2), "
    "  (10, 70, 70, 3, false, '2017-02-27T19:33:00Z', 3221225472, 2); "

    "INSERT INTO current_ways (id, changeset_id, \"timestamp\", visible, version) "
    "VALUES "
    "  (1, 3, '2017-02-27T19:33:00Z', true, 2); "

    "INSERT INTO current_way_nodes (way_id, node_id, sequence_id) "
    "VALUES "
    "  (1,  3,  1), "
    "  (1,  4,  2), "
    "  (1,  5,  3), "
    "  (1,  6,  4), "
    "  (1,  7,  5), "
    "  (1,  8,  6), "
    "  (1,  9,  7), "
    "  (1, 10,  8); "

    "INSERT INTO ways (way_id, changeset_id, \"timestamp\", visible, "
    "                  version, redaction_id) "
    "VALUES "
    "  (1, 3, '2017-02-27T19:33:00Z', true, 2, NULL), "
    "  (1, 3, '2017-02-27T19:33:00Z', true, 1, NULL); "

    "INSERT INTO way_nodes (way_id, version, node_id, sequence_id) "
    "VALUES "
    "  (1, 1,  3,  8), "
    "  (1, 1,  4,  7), "
    "  (1, 1,  5,  6), "
    "  (1, 1,  6,  5), "
    "  (1, 1,  7,  4), "
    "  (1, 1,  8,  3), "
    "  (1, 1,  9,  2), "
    "  (1, 1, 10,  1), "
    "  (1, 2,  3,  1), "
    "  (1, 2,  4,  2), "
    "  (1, 2,  5,  3), "
    "  (1, 2,  6,  4), "
    "  (1, 2,  7,  5), "
    "  (1, 2,  8,  6), "
    "  (1, 2,  9,  7), "
    "  (1, 2, 10,  8); "
    "");
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(
    sel->supports_historical_versions(), true,
    "data selection supports historical versions");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  assert_equal<int>(
    sel->select_ways_with_history(ids), 2,
    "number of way versions with history selected");

  test_formatter f;
  sel->write_ways(f);
  assert_equal<size_t>(f.m_ways.size(), 2, "number of ways written");

  nodes_t way_v1_nds({10, 9, 8, 7, 6, 5, 4, 3});
  nodes_t way_v2_nds({3, 4, 5, 6, 7, 8, 9, 10});

  assert_equal<test_formatter::way_t>(
    test_formatter::way_t(
      element_info(1, 1, 3, "2017-02-27T19:33:00Z", 1, std::string("user_1"), true),
      way_v1_nds,
      tags_t()
      ),
    f.m_ways[0], "first way written");

  assert_equal<test_formatter::way_t>(
    test_formatter::way_t(
      element_info(1, 2, 3, "2017-02-27T19:33:00Z", 1, std::string("user_1"), true),
      way_v2_nds,
      tags_t()
      ),
    f.m_ways[1], "second way written");
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(boost::function<void(test_database&)>(
        &test_historic_elements));

    tdb.run(boost::function<void(test_database&)>(
        &test_historic_dup));

    tdb.run(boost::function<void(test_database&)>(
        &test_historic_dup_way));

    tdb.run(boost::function<void(test_database&)>(
        &test_historic_dup_relation));

    tdb.run(boost::function<void(test_database&)>(
        &test_node_history));

    tdb.run(boost::function<void(test_database&)>(
        &test_way_history));

    tdb.run(boost::function<void(test_database&)>(
        &test_relation_history));

    tdb.run(boost::function<void(test_database&)>(
        &test_node_with_history_redacted));

    tdb.run(boost::function<void(test_database&)>(
        &test_historical_nodes_redacted));

    tdb.run(boost::function<void(test_database&)>(
        &test_way_with_history_redacted));

    tdb.run(boost::function<void(test_database&)>(
        &test_historical_ways_redacted));

    tdb.run(boost::function<void(test_database&)>(
        &test_relation_with_history_redacted));

    tdb.run(boost::function<void(test_database&)>(
        &test_historical_relations_redacted));

    tdb.run(boost::function<void(test_database&)>(
        &test_historic_way_node_order));

  } catch (const test_database::setup_error &e) {
    std::cout << "Unable to set up test database: " << e.what() << std::endl;
    return 77;

  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cout << "Unknown exception type." << std::endl;
    return 99;
  }

  return 0;
}
