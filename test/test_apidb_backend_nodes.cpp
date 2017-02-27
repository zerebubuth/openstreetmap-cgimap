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

void test_single_nodes(test_database &tdb) {
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
  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  if (sel->check_node_visibility(1) != data_selection::exists) {
    throw std::runtime_error("Node 1 should be visible, but isn't");
  }
  if (sel->check_node_visibility(2) != data_selection::exists) {
    throw std::runtime_error("Node 2 should be visible, but isn't");
  }

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  ids.push_back(2);
  ids.push_back(3);
  ids.push_back(4);
  if (sel->select_nodes(ids) != 4) {
    throw std::runtime_error("Selecting 4 nodes failed");
  }
  if (sel->select_nodes(ids) != 0) {
    throw std::runtime_error("Re-selecting 4 nodes failed");
  }

  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(1), data_selection::exists,
    "node 1 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(2), data_selection::exists,
    "node 2 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(3), data_selection::deleted,
    "node 3 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(4), data_selection::exists,
    "node 4 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(5), data_selection::non_exist,
    "node 5 visibility");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 4, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(2, 1, 1, "2013-11-14T02:10:01Z", 1, std::string("user_1"), true),
      0.1, 0.1,
      tags_t()
      ),
    f.m_nodes[1], "second node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[2], "third node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(4, 1, 4, "2015-03-02T19:25:00Z", boost::none, boost::none, true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[3], "fourth (anonymous) node written");
}

void test_dup_nodes(test_database &tdb) {
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

  boost::shared_ptr<data_selection> sel = tdb.get_data_selection();

  if (sel->check_node_visibility(1) != data_selection::exists) {
    throw std::runtime_error("Node 1 should be visible, but isn't");
  }

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  ids.push_back(1);
  ids.push_back(1);
  if (sel->select_nodes(ids) != 1) {
    throw std::runtime_error("Selecting 3 duplicates of 1 node failed");
  }
  if (sel->select_nodes(ids) != 0) {
    throw std::runtime_error("Re-selecting the same node failed");
  }

  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(1), data_selection::exists,
    "node 1 visibility");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 1, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
}

void test_psql_array_to_vector() {
  std::string test = "{NULL}";
  std::vector<std::string> actual_values;
  std::vector<std::string> values = psql_array_to_vector(test);

  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test);
  }

  test = "{1,2}";
  values = psql_array_to_vector(test);
  actual_values.clear();
  actual_values.push_back("1");
  actual_values.push_back("2");
  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test);
  }

  test = "{\"TEST\",TEST123}";
  values = psql_array_to_vector(test);
  actual_values.clear();
  actual_values.push_back("TEST");
  actual_values.push_back("TEST123");
  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test);
  }

  test = "{\"},\\\"\",\",{}}\\\\\"}";
  values = psql_array_to_vector(test);
  actual_values.clear();
  actual_values.push_back("},\"");
  actual_values.push_back(",{}}\\");
  if (values != actual_values) {
    throw std::runtime_error("Psql array parse failed for " + test + " " +values[0] + " " +values[1]);
  }
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    test_psql_array_to_vector();

    tdb.run(boost::function<void(test_database&)>(
        &test_single_nodes));

    tdb.run(boost::function<void(test_database&)>(
        &test_dup_nodes));

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
