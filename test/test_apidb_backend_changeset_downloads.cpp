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

void test_changeset_select_node(test_database &tdb) {
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
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(sel->supports_changesets(), true,
    "apidb should support changesets.");
  assert_equal<bool>(sel->supports_historical_versions(), true,
    "apidb should support historical versions.");

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(1);
  int num = sel->select_historical_by_changesets(ids);
  assert_equal<int>(num, 1, "should have selected one element from changeset 1");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 1,
    "should have written one node from changeset 1");

  assert_equal<test_formatter::node_t>(
    f.m_nodes.front(),
    test_formatter::node_t(
      element_info(1, 1, 1, "2017-03-19T19:13:00Z", 1, std::string("user_1"), true),
      9.0, 9.0,
      tags_t()),
    "node 1 in changeset 1");
}

void test_changeset_select_way(test_database &tdb) {
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
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  assert_equal<bool>(sel->supports_changesets(), true,
    "apidb should support changesets.");
  assert_equal<bool>(sel->supports_historical_versions(), true,
    "apidb should support historical versions.");

  std::vector<osm_changeset_id_t> ids;
  ids.push_back(2);
  int num = sel->select_historical_by_changesets(ids);
  assert_equal<int>(num, 2, "number of ways (2) selected from changeset 1");

  test_formatter f;
  sel->write_ways(f);
  assert_equal<size_t>(f.m_ways.size(), 2,
    "number of ways (2) written from changeset 1");

  assert_equal<test_formatter::way_t>(
    f.m_ways[0],
    test_formatter::way_t(
      element_info(1, 1, 2, "2017-03-19T19:57:00Z", 1, std::string("user_1"), true),
      nodes_t{1},
      tags_t()),
    "way 1, version 1 in changeset 1");
  assert_equal<test_formatter::way_t>(
    f.m_ways[1],
    test_formatter::way_t(
      element_info(1, 2, 2, "2017-03-19T19:57:00Z", 1, std::string("user_1"), true),
      nodes_t{1},
      tags_t()),
    "way 1, version 2 in changeset 1");
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(boost::function<void(test_database&)>(
        &test_changeset_select_node));

    tdb.run(boost::function<void(test_database&)>(
        &test_changeset_select_way));

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
