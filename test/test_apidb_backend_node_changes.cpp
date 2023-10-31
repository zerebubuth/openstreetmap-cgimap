#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"
#include "test_request.hpp"

namespace {

void test_end_to_end(test_database &tdb) {

  // Prepare users, changesets

  tdb.run_sql(R"(
    INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
    VALUES
      (1, 'demo@example.com', '3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=',
                              'sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=',
                              '2013-11-14T02:10:00Z', 'demo', true, 'confirmed');

    INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
    VALUES
      (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0);
  )");

  const std::string baseauth = "Basic ZGVtbzpwYXNzd29yZA==";
  const std::string generator = "Test";

  auto sel_factory = tdb.get_data_selection_factory();
  auto upd_factory = tdb.get_data_update_factory();

  null_rate_limiter limiter;
  routes route;

  // create a node without tags
  {
    test_request req;
    req.set_header("REQUEST_METHOD", "PUT");
    req.set_header("REQUEST_URI", "/api/0.6/node/create");
    req.set_header("HTTP_AUTHORIZATION", baseauth);
    req.set_header("REMOTE_ADDR", "127.0.0.1");
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
      <osm>
        <node lon="12" lat="34" changeset="1"/>
      </osm>)");
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get(), nullptr);

    if (req.response_status() != 200)
      throw std::runtime_error("Expected HTTP 200 OK: Create new node");
  }
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();
    tdb.run_update(std::function<void(test_database&)>(&test_end_to_end));
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
