/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */


#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"
#include "cgimap/request_context.hpp"

#include "test_database.hpp"
#include "test_request.hpp"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

using Catch::Matchers::Equals;


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

  void testRunStarting(Catch::TestRunInfo const& testRunInfo ) override {
    // disable db write operations
    tdb.add_vm_param("disable-api-write", true);
    // load database schema when starting up tests
    tdb.setup(test_db_sql);
  }

  void testCaseStarting(Catch::TestCaseInfo const& testInfo ) override {
    tdb.testcase_starting();
  }

  void testCaseEnded(Catch::TestCaseStats const& testCaseStats ) override {
    tdb.testcase_ended();
  }
};

CATCH_REGISTER_LISTENER( CGImapListener )

TEST_CASE_METHOD( DatabaseTestsFixture, "test_disabled_api_write", "[disableapiwrite][db]" ) {

  SECTION("Initialize test data") {

    tdb.run_sql(R"(
      INSERT INTO users (id, email, pass_crypt, pass_salt, creation_time, display_name, data_public, status)
      VALUES
        (1, 'demo@example.com', 'x', '', '2013-11-14T02:10:00Z', 'demo', true, 'confirmed');

     INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
     VALUES
       (1, 1, now() at time zone 'utc', now() at time zone 'utc' + '1 hour' ::interval, 0);;

     INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at)
      VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c',
             'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');

     INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
      VALUES (67, 1, 3, '4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8', NULL, NULL, NULL, '2021-04-14 19:38:21', 'write_api', '');
     )"
   );
  }

  SECTION("Try to upload a node, API write disabled")
  {
    const std::string bearertoken = "Bearer 4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8";
    const std::string generator = "Test";

    null_rate_limiter limiter;
    routes route;
    test_request req;

    req.set_header("REQUEST_METHOD", "POST");
    req.set_header("REQUEST_URI", "/api/0.6/changeset/1/upload");
    req.set_header("REMOTE_ADDR", "127.0.0.1");
    req.set_header("HTTP_AUTHORIZATION", bearertoken);

    // set up request headers from test case
    req.set_payload(R"(<?xml version="1.0" encoding="UTF-8"?>
                <osmChange version="0.6" generator="iD">
                <create>
                  <node id="-5" lon="11" lat="46" version="0" changeset="1">
                     <tag k="highway" v="bus_stop" />
                  </node>
               </create>
               </osmChange>)" );

    auto sel_factory = tdb.get_data_selection_factory();
    auto upd_factory = tdb.get_data_update_factory();

    // execute the request
    process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

    CAPTURE(req.body().str());

    REQUIRE(req.response_status() == 400);

    REQUIRE_THAT(req.body().str(), Equals("Server is currently in read only mode, no database changes allowed at this time"));
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

  if (int returnCode = session.applyCommandLine(argc, argv); returnCode != 0)
    return returnCode;

  if (!test_db_sql.empty())
    DatabaseTestsFixture::setTestDatabaseSchema(test_db_sql);

  return session.run();
}
