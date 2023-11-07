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
    throw std::runtime_error(fmt::format("Expecting {} to be equal, but {} != {}", message, a, b));
  }
}

void test_end_to_end(test_database &tdb, const std::string& title, const std::string& payload, double target_lat, double target_lon, const tags_t& target_tags) {

  // Prepare users, changesets

  tdb.run_sql(R"(
    INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
    VALUES
      (1, 'demo@example.com', '3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=',
                              'sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=',
                              '2013-11-14T02:10:00Z', 'demo', true, 'confirmed');

    INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
    VALUES
      (1, 1, now() at time zone 'utc' - '12 hour' ::interval,
             now() at time zone 'utc' - '11 hour' ::interval, 0),
      (2, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0);
  )");

  const std::string baseauth = "Basic ZGVtbzpwYXNzd29yZA==";
  const std::string generator = "Test";
  const osm_nwr_id_t target_version = 1;
  const osm_changeset_id_t target_changeset_id = 2;
  const osm_user_id_t target_user_id = 1;
  const std::string target_display_name = "demo";

  auto sel_factory = tdb.get_data_selection_factory();
  auto upd_factory = tdb.get_data_update_factory();

  null_rate_limiter limiter;
  routes route;

  assert_equal<int>(
    tdb.run_sql("SELECT id FROM current_nodes"), 0,
    fmt::format("number of nodes before writing {}", title));

  test_request req;
  req.set_header("REQUEST_METHOD", "PUT");
  req.set_header("REQUEST_URI", "/api/0.6/node/create");
  req.set_header("HTTP_AUTHORIZATION", baseauth);
  req.set_header("REMOTE_ADDR", "127.0.0.1");
  req.set_payload(payload);
  process_request(req, limiter, generator, route, *sel_factory, upd_factory.get(), nullptr);

  if (req.response_status() != 200)
    throw std::runtime_error(fmt::format("Expected HTTP 200 OK: Create {}", title));

  assert_equal<int>(
    tdb.run_sql("SELECT id FROM current_nodes"), 1,
    fmt::format("number of nodes after writing {}", title));

  osm_nwr_id_t node_id;
  req.body() >> node_id;
  auto sel = tdb.get_data_selection();
  
  if (sel->check_node_visibility(node_id) != data_selection::exists)
    throw std::runtime_error(fmt::format("{} should be visible, but isn't", title));

  sel->select_nodes({ node_id });
  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 1, fmt::format("number of nodes written for {}", title));
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(node_id, target_version, target_changeset_id, f.m_nodes[0].elem.timestamp, target_user_id, target_display_name, true),
      target_lon, target_lat, target_tags
    ),
    f.m_nodes[0], title);
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();
    tdb.run_update([](test_database &tdb) {
      test_end_to_end(tdb, "node without tags",
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <osm>
          <node lat="12" lon="34" changeset="2"/>
        </osm>)",
        12, 34, tags_t());
    });
    tdb.run_update([](test_database &tdb) {
      test_end_to_end(tdb, "node with tags",
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <osm>
          <node lat="21" lon="43" changeset="2">
            <tag k="natural" v="tree"/>
            <tag k="height" v="19"/>
          </node>
        </osm>)",
        21, 43, tags_t({{"natural", "tree"}, {"height", "19"}}));
    });
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
