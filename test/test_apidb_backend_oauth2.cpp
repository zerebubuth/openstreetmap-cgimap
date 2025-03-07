/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include <sys/time.h>
#include <cstdio>

#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"

#include "test_database.hpp"
#include "test_request.hpp"


#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

namespace {

struct recording_rate_limiter : public rate_limiter {
  ~recording_rate_limiter() override = default;

  std::tuple<bool, int> check(const std::string &key, bool moderator) override {
    m_keys_seen.insert(key);
    return std::make_tuple(false, 0);
  }

  void update(const std::string &key, uint32_t bytes, bool moderator) override {
    m_keys_seen.insert(key);
  }

  bool saw_key(const std::string &key) const {
    return m_keys_seen.count(key) > 0;
  }

private:
  std::set<std::string> m_keys_seen;
};

void add_common_headers(test_request& req)
{
  req.set_header("HTTP_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_ACCEPT_ENCODING", "gzip;q=1.0, deflate;q=0.6, identity;q=0.3");
  req.set_header("HTTP_ACCEPT", "*/*");
  req.set_header("HTTP_USER_AGENT", "OAuth gem v0.4.7");
  req.set_header("HTTP_X_REQUEST_ID", "V-eaKX8AAQEAAF4UzHwAAAHt");
  req.set_header("HTTP_X_FORWARDED_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_X_FORWARDED_SERVER", "www.openstreetmap.org");
  req.set_header("HTTP_CONNECTION", "Keep-Alive");
  req.set_header("SERVER_SIGNATURE", "<address>Apache/2.4.18 (Ubuntu) Server "
                 "at www.openstreetmap.org Port 80</address>");
  req.set_header("SERVER_SOFTWARE", "Apache/2.4.18 (Ubuntu)");
  req.set_header("SERVER_NAME", "www.openstreetmap.org");
  req.set_header("SERVER_ADDR", "127.0.0.1");
  req.set_header("SERVER_PORT", "80");
  req.set_header("REMOTE_ADDR", "127.0.0.1");
  req.set_header("DOCUMENT_ROOT", "/srv/www.openstreetmap.org/rails/public");
  req.set_header("REQUEST_SCHEME", "http");
  req.set_header("SERVER_PROTOCOL", "HTTP/1.1");
}


int create_changeset(test_database &tdb, const std::string &token) {

  // Test valid token, create empty changeset
  recording_rate_limiter limiter;
  std::string generator("test_apidb_backend.cpp");
  routes route;

  auto sel_factory = tdb.get_data_selection_factory();
  auto upd_factory = tdb.get_data_update_factory();

  test_request req;
  req.set_header("SCRIPT_URL", "/api/0.6/changeset/create");
  req.set_header("SCRIPT_URI", "http://www.openstreetmap.org/api/0.6/changeset/create");
  req.set_header("HTTP_AUTHORIZATION","Bearer " + token);
  req.set_header("REQUEST_METHOD", "PUT");
  req.set_header("QUERY_STRING", "");
  req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
  req.set_header("SCRIPT_NAME", "/api/0.6/changeset/create");
  add_common_headers(req);

  req.set_payload(R"( <osm><changeset><tag k="created_by" v="JOSM 1.61"/><tag k="comment" v="Just adding some streetnames"/></changeset></osm> )" );

  process_request(req, limiter, generator, route, *sel_factory, upd_factory.get());

  return (req.response_status());
}

int fetch_relation(test_database &tdb, const std::string &token) {

  recording_rate_limiter limiter;
  std::string generator("test_apidb_backend.cpp");
  routes route;
  auto sel_factory = tdb.get_data_selection_factory();

  test_request req;
  req.set_header("SCRIPT_URL", "/api/0.6/relation/165475/full");
  req.set_header("SCRIPT_URI", "http://www.openstreetmap.org/api/0.6/relation/165475/full");
  req.set_header("HTTP_AUTHORIZATION","Bearer " + token);
  req.set_header("REQUEST_METHOD", "GET");
  req.set_header("QUERY_STRING", "");
  req.set_header("REQUEST_URI", "/api/0.6/relation/165475/full");
  req.set_header("SCRIPT_NAME", "/api/0.6/relation/165475/full");
  add_common_headers(req);

  process_request(req, limiter, generator, route, *sel_factory, nullptr);

  return (req.response_status());
}

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



TEST_CASE_METHOD(DatabaseTestsFixture, "test_user_id_for_oauth2_token", "[oauth2][db]" ) {

  bool allow_api_write;
  bool expired;
  bool revoked;

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {

    tdb.run_sql(R"(

      INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public)
      VALUES 
        (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true),
        (2, 'user_2@example.com', '', '2021-03-12T01:33:43Z', 'user_2', true);
  
     INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at) 
         VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c', 
                'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');
  
     INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at)
         VALUES (4, 'User', 2, 'App 2', 'WNr9KjjzA9uNCXXBHG1AReR2jdottwlKYOz7CLgjUAk', 'cdd6f17bc32eb96b33839db59ae5873777e95864cd936ae445f2dedec8787212',
                'http://localhost:3000/demo', 'write_prefs write_diary', true, '2021-04-13 18:59:11', '2021-04-13 18:59:11');
  
     INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (67, 1, 3, '4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8', NULL, NULL, NULL, '2021-04-14 19:38:21', 'write_api', '');
  
     INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (68, 1, 3, '1187c28b93ab4a14e3df6a61ef46a24d7d4d7964c1d56eb2bfd197b059798c1d', NULL, NULL, '2021-04-15 06:11:01', '2021-04-14 22:06:58', 'write_api', '');
  
     INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (69, 1, 3, '9d3e411efa288369a509d8798d17b2a669f331452cdd5d86cd696dad46517e6d', NULL, NULL, NULL, '2021-04-14 19:38:21', 'read_prefs write_api', '');
  
     INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (70, 1, 3, 'e466d2ba2ff5da35fdaa7547eb6c27ae0461c7a4acc05476c0a33b1b1d0788cd', NULL, NULL, NULL, '2021-04-14 19:38:21', 'read_prefs read_gpx', '');
  
     INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (71, 1, 3, 'f0e6f310ee3a9362fe00cee4328ad318a1fa6c770b2e19975271da99a6407476', NULL, 3600, NULL, now() at time zone 'utc' - '2 hours' :: interval, 'write_api', '');
  
     INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (72, 1, 3, 'b1294a183bf64f4d9a97f24ed84ce88e3ab6e7ada78114d6e600bdb63831237b', NULL, 3600, NULL, now() at time zone 'utc' - '30 minutes' :: interval, 'write_api', ''); 

    )");
  }

  // Note: Tokens in this unit tests are considered to be opaque strings, tokens are used for db lookups as-is.
  // It doesn't matter if they have been previously stored as plain or sha256-hashed tokens.
  // Also see test_oauth2.cpp for oauth2::validate_bearer_token tests, which include auth token hash calculation

  SECTION("Valid token w/ write API scope") {
    const auto user_id = sel->get_user_id_for_oauth2_token("4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8", expired, revoked, allow_api_write);
    CHECK(user_id == 1);
    REQUIRE(allow_api_write);
    REQUIRE_FALSE(expired);
    REQUIRE_FALSE(revoked);
  }

  SECTION("Invalid (non existing) token") {
    const auto user_id = sel->get_user_id_for_oauth2_token("a6ee343e3417915c87f492aac2a7b638647ef576e2a03256bbf1854c7e06c163", expired, revoked, allow_api_write);
    REQUIRE_FALSE(user_id.has_value());
  }

  SECTION("Revoked token") {
    const auto user_id = sel->get_user_id_for_oauth2_token("1187c28b93ab4a14e3df6a61ef46a24d7d4d7964c1d56eb2bfd197b059798c1d", expired, revoked, allow_api_write);
    CHECK(user_id == 1);
    REQUIRE(allow_api_write);
    REQUIRE_FALSE(expired);
    REQUIRE(revoked);
  }

  SECTION("Two scopes, including write_api") {
    const auto user_id = sel->get_user_id_for_oauth2_token("4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8", expired, revoked, allow_api_write);
    CHECK(user_id == 1);
    REQUIRE(allow_api_write);
    REQUIRE_FALSE(expired);
    REQUIRE_FALSE(revoked);
  }

  SECTION("Two scopes, not write_api") {
    const auto user_id = sel->get_user_id_for_oauth2_token("e466d2ba2ff5da35fdaa7547eb6c27ae0461c7a4acc05476c0a33b1b1d0788cd", expired, revoked, allow_api_write);
    CHECK(user_id == 1);
    REQUIRE_FALSE(allow_api_write);
    REQUIRE_FALSE(expired);
    REQUIRE_FALSE(revoked);
  }

  SECTION("expired token") {
    const auto user_id = sel->get_user_id_for_oauth2_token("f0e6f310ee3a9362fe00cee4328ad318a1fa6c770b2e19975271da99a6407476", expired, revoked, allow_api_write);
    CHECK(user_id == 1);
    REQUIRE(allow_api_write);
    REQUIRE(expired);
    REQUIRE_FALSE(revoked);
  }

  SECTION("token to expire in about 30 minutes") {
    const auto user_id = sel->get_user_id_for_oauth2_token("b1294a183bf64f4d9a97f24ed84ce88e3ab6e7ada78114d6e600bdb63831237b", expired, revoked, allow_api_write);
    CHECK(user_id == 1);
    REQUIRE(allow_api_write);
    REQUIRE_FALSE(expired);
    REQUIRE_FALSE(revoked);
  }
}


TEST_CASE_METHOD(DatabaseTestsFixture, "test_oauth2_end_to_end", "[oauth2][db]" ) {

  // tokens 1yi2RI2W... and 2Kx... are stored in plain text in oauth_access_tokens table, all others as sha256-hash value

  // Column status in table users is for information purposes only
  // User id 1000 denotes an inactive user (see empty_data_selection, method is_user_active)

  SECTION("Initialize test data") {

    tdb.run_sql(R"(

      INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public, status)
      VALUES 
        (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true, 'confirmed'),
        (2, 'user_2@example.com', '', '2021-03-12T01:33:43Z', 'user_2', true, 'active'),
        (1000, 'user_1000@example.com', '', '2021-04-12T01:33:43Z', 'user_1000', true, 'pending');
  
      INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at) 
         VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c', 
                'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');
  
      INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (67, 1, 3, '1yi2RI2WhIVMLoLaDLg0nrPJPU4WQSIX4Hh_jxfRRxI', NULL, NULL, NULL, '2021-04-14 19:38:21.991429', 'write_api', '');
  
      INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (72, 1, 3, '4ea5b956c8882db030a5a799cb45eb933bb6dd2f196a44f68167d96fbc8ec3f1', NULL, NULL, NULL, '2021-04-14 19:38:21.991429', 'read_prefs', '');
  
      INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
         VALUES (78, 1000, 3, '2KxONxvhoSji9F8dz_WO6UZOzRdmQ0ISB0ovnZrJnhM', NULL, NULL, NULL, '2021-04-14 19:38:21.991429', 'write_api', '');
    )");
  }

  SECTION("Fetch relation: test valid token -> HTTP 404 not found, due to unknown relation") {
    CHECK(fetch_relation(tdb, "1yi2RI2WhIVMLoLaDLg0nrPJPU4WQSIX4Hh_jxfRRxI") == 404);
  }

  SECTION("Fetch relation: test unknown token -> HTTP 401 Unauthorized") {
    CHECK(fetch_relation(tdb, "8JrrmoKSUtzBhmenUUQF27PVdQn2QY8YdRfosu3R-Dc") == 401);
  }

  // Test valid token, create empty changeset

  // missing write_api scope --> http::unauthorized ("You have not granted the modify map permission")
  SECTION("Create empty changeset: missing write_api scope") {
    CHECK(create_changeset(tdb, "hCXrz5B5fCBHusp0EuD2IGwYSxS8bkAnVw2_aLEdxig") == 401);
  }

  SECTION("Create empty changeset: includes write_api scope") {
    CHECK(create_changeset(tdb, "1yi2RI2WhIVMLoLaDLg0nrPJPU4WQSIX4Hh_jxfRRxI") == 200);
  }

  // Same as previous tests case. However, user 1000 is not active this time
  // Creating changesets should be rejected with HTTP 403 (Forbidden)
  SECTION("Create empty changeset: includes write_api scope, user not active") {
    CHECK(create_changeset(tdb, "2KxONxvhoSji9F8dz_WO6UZOzRdmQ0ISB0ovnZrJnhM") == 403);
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
