#include <iostream>
#include <stdexcept>
#include <fmt/core.h>
#include <boost/program_options.hpp>

#include <sys/time.h>
#include <cstdio>

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

template<typename T>
std::ostream& operator<<(std::ostream& os, std::optional<T> const& opt)
{
  return opt ? os << opt.value() : os;
}

}

template <> struct fmt::formatter<std::optional<osm_user_id_t>> {
  template <typename FormatContext>
  auto format(const std::optional<osm_user_id_t>& v, FormatContext& ctx) -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    std::ostringstream ostr;
    ostr << v;
    return format_to(ctx.out(), "{}", ostr.str());
  }
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
};


namespace {

template <typename T>
void assert_equal(const T& a, const T&b, const std::string &message) {
  if (a != b) {
    throw std::runtime_error(fmt::format("Expecting {} to be equal, but {} != {}", message, a, b));
  }
}



void test_user_id_for_oauth2_token(test_database &tdb) {
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

  auto store = tdb.get_oauth_store();

  // Note: Tokens in this unit tests are considered to be opaque strings, tokens are used for db lookups as-is.
  // It doesn't matter if they have been previously stored as plain or sha256-hashed tokens.
  // Also see test_oauth2.cpp for oauth2::validate_bearer_token tests, which include auth token hash calculation

  // Valid token w/ write API scope
  {
    bool allow_api_write;
    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token("4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8", expired, revoked, allow_api_write);
    assert_equal<std::optional<osm_user_id_t>>(user_id, 1, "test_apidb_backend_oauth2::001 - user id");
    assert_equal<bool>(allow_api_write, true, "test_apidb_backend_oauth2::001 - allow_api_write");
    assert_equal<bool>(expired, false, "test_apidb_backend_oauth2::001 - expired");
    assert_equal<bool>(revoked, false, "test_apidb_backend_oauth2::001 - revoked");
  }

  // Invalid (non existing) token
  {
    bool allow_api_write;
    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token("a6ee343e3417915c87f492aac2a7b638647ef576e2a03256bbf1854c7e06c163", expired, revoked, allow_api_write);
    assert_equal<bool>(user_id.has_value(), false, "test_apidb_backend_oauth2::002");
  }

  // Revoked token
  {
    bool allow_api_write;
    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token("1187c28b93ab4a14e3df6a61ef46a24d7d4d7964c1d56eb2bfd197b059798c1d", expired, revoked, allow_api_write);
    assert_equal<std::optional<osm_user_id_t>>(user_id, 1, "test_apidb_backend_oauth2::003 - user id");
    assert_equal<bool>(allow_api_write, true, "test_apidb_backend_oauth2::003 - allow_api_write");
    assert_equal<bool>(expired, false, "test_apidb_backend_oauth2::003 - expired");
    assert_equal<bool>(revoked, true, "test_apidb_backend_oauth2::003 - revoked");
  }

  // Two scopes, including write_api
  {
    bool allow_api_write;
    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token("4f41f2328befed5a33bcabdf14483081c8df996cbafc41e313417776e8fafae8", expired, revoked, allow_api_write);
    assert_equal<std::optional<osm_user_id_t>>(user_id, 1, "test_apidb_backend_oauth2::004 - user id");
    assert_equal<bool>(allow_api_write, true, "test_apidb_backend_oauth2::004 - allow_api_write");
    assert_equal<bool>(expired, false, "test_apidb_backend_oauth2::004 - expired");
    assert_equal<bool>(revoked, false, "test_apidb_backend_oauth2::004 - revoked");
  }

  // Two scopes, not write_api
  {
    bool allow_api_write;
    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token("e466d2ba2ff5da35fdaa7547eb6c27ae0461c7a4acc05476c0a33b1b1d0788cd", expired, revoked, allow_api_write);
    assert_equal<std::optional<osm_user_id_t>>(user_id, 1, "test_apidb_backend_oauth2::005 - user id");
    assert_equal<bool>(allow_api_write, false, "test_apidb_backend_oauth2::005 - allow_api_write");
    assert_equal<bool>(expired, false, "test_apidb_backend_oauth2::005 - expired");
    assert_equal<bool>(revoked, false, "test_apidb_backend_oauth2::005 - revoked");
  }

  // expired token
  {
    bool allow_api_write;
    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token("f0e6f310ee3a9362fe00cee4328ad318a1fa6c770b2e19975271da99a6407476", expired, revoked, allow_api_write);
    assert_equal<std::optional<osm_user_id_t>>(user_id, 1, "test_apidb_backend_oauth2::006 - user id");
    assert_equal<bool>(allow_api_write, true, "test_apidb_backend_oauth2::006 - allow_api_write");
    assert_equal<bool>(expired, true, "test_apidb_backend_oauth2::006 - expired");
    assert_equal<bool>(revoked, false, "test_apidb_backend_oauth2::006 - revoked");
  }

  // token to expire in about 30 minutes
  {
    bool allow_api_write;
    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token("b1294a183bf64f4d9a97f24ed84ce88e3ab6e7ada78114d6e600bdb63831237b", expired, revoked, allow_api_write);
    assert_equal<std::optional<osm_user_id_t>>(user_id, 1, "test_apidb_backend_oauth2::006 - user id");
    assert_equal<bool>(allow_api_write, true, "test_apidb_backend_oauth2::007 - allow_api_write");
    assert_equal<bool>(expired, false, "test_apidb_backend_oauth2::007 - expired");
    assert_equal<bool>(revoked, false, "test_apidb_backend_oauth2::007 - revoked");
  }

}

class empty_data_selection
  : public data_selection {
public:

  virtual ~empty_data_selection() = default;

  void write_nodes(output_formatter &formatter) override {}
  void write_ways(output_formatter &formatter) override {}
  void write_relations(output_formatter &formatter) override {}
  void write_changesets(output_formatter &formatter,
                        const std::chrono::system_clock::time_point &now) override {}

  visibility_t check_node_visibility(osm_nwr_id_t id) override { return non_exist; }
  visibility_t check_way_visibility(osm_nwr_id_t id) override { return non_exist; }
  visibility_t check_relation_visibility(osm_nwr_id_t id) override { return non_exist; }

  int select_nodes(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_ways(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_relations(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_nodes_from_bbox(const bbox &bounds, int max_nodes) override { return 0; }
  void select_nodes_from_relations() override {}
  void select_ways_from_nodes() override {}
  void select_ways_from_relations() override {}
  void select_relations_from_ways() override {}
  void select_nodes_from_way_nodes() override {}
  void select_relations_from_nodes() override {}
  void select_relations_from_relations(bool drop_relations = false) override {}
  void select_relations_members_of_relations() override {}
  int select_changesets(const std::vector<osm_changeset_id_t> &) override { return 0; }
  void select_changeset_discussions() override {}
  void drop_nodes() override {}
  void drop_ways() override {}
  void drop_relations() override {}

  bool supports_user_details() const override { return false; }
  bool is_user_blocked(const osm_user_id_t) override { return true; }
  bool get_user_id_pass(const std::string&, osm_user_id_t &, std::string &, std::string &) override { return false; };

  int select_historical_nodes(const std::vector<osm_edition_t> &) override { return 0; }
  int select_nodes_with_history(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_historical_ways(const std::vector<osm_edition_t> &) override { return 0; }
  int select_ways_with_history(const std::vector<osm_nwr_id_t> &) override { return 0; }
  int select_historical_relations(const std::vector<osm_edition_t> &) override { return 0; }
  int select_relations_with_history(const std::vector<osm_nwr_id_t> &) override { return 0; }
  void set_redactions_visible(bool) override {}
  int select_historical_by_changesets(const std::vector<osm_changeset_id_t> &) override { return 0; }

  struct factory
    : public data_selection::factory {
    virtual ~factory() = default;
    virtual std::unique_ptr<data_selection> make_selection(Transaction_Owner_Base&) {
      return std::make_unique<empty_data_selection>();
    }
    virtual std::unique_ptr<Transaction_Owner_Base> get_default_transaction() {
      {
        return std::unique_ptr<Transaction_Owner_Void>(new Transaction_Owner_Void());
      }
    }
  };
};

struct recording_rate_limiter
  : public rate_limiter {
  ~recording_rate_limiter() = default;

  bool check(const std::string &key) {
    m_keys_seen.insert(key);
    return true;
  }

  void update(const std::string &key, int bytes) {
    m_keys_seen.insert(key);
  }

  bool saw_key(const std::string &key) {
    return m_keys_seen.count(key) > 0;
  }

private:
  std::set<std::string> m_keys_seen;
};


void create_changeset(test_database &tdb, oauth::store& store, std::string token, int expected_response_code) {

  // Test valid token, create empty changeset
  recording_rate_limiter limiter;
  std::string generator("test_apidb_backend.cpp");
  routes route;

  auto sel_factory = tdb.get_data_selection_factory();
  auto upd_factory = tdb.get_data_update_factory();

  test_request req;
  req.set_header("SCRIPT_URL", "/api/0.6/changeset/create");
  req.set_header("SCRIPT_URI",
		 "http://www.openstreetmap.org/api/0.6/changeset/create");
  req.set_header("HTTP_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_ACCEPT_ENCODING",
		 "gzip;q=1.0,deflate;q=0.6,identity;q=0.3");
  req.set_header("HTTP_ACCEPT", "*/*");
  req.set_header("HTTP_USER_AGENT", "OAuth gem v0.4.7");
  req.set_header("HTTP_AUTHORIZATION","Bearer " + token);
  req.set_header("HTTP_X_REQUEST_ID", "V-eaKX8AAQEAAF4UzHwAAAHt");
  req.set_header("HTTP_X_FORWARDED_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_X_FORWARDED_SERVER", "www.openstreetmap.org");
  req.set_header("HTTP_CONNECTION", "Keep-Alive");
  req.set_header("SERVER_NAME", "www.openstreetmap.org");
  req.set_header("SERVER_ADDR", "127.0.0.1");
  req.set_header("SERVER_PORT", "80");
  req.set_header("REMOTE_ADDR", "127.0.0.1");
  req.set_header("DOCUMENT_ROOT", "/srv/www.openstreetmap.org/rails/public");
  req.set_header("REQUEST_SCHEME", "http");
  req.set_header("SERVER_PROTOCOL", "HTTP/1.1");
  req.set_header("REQUEST_METHOD", "PUT");
  req.set_header("QUERY_STRING", "");
  req.set_header("REQUEST_URI", "/api/0.6/changeset/create");
  req.set_header("SCRIPT_NAME", "/api/0.6/changeset/create");

  req.set_payload(R"( <osm><changeset><tag k="created_by" v="JOSM 1.61"/><tag k="comment" v="Just adding some streetnames"/></changeset></osm> )" );

  process_request(req, limiter, generator, route, *sel_factory, upd_factory.get(), &store);

  assert_equal<int>(expected_response_code, req.response_status(), "response status");
}

void fetch_relation(test_database &tdb, oauth::store& store, std::string token, int expected_response_code) {

  recording_rate_limiter limiter;
  std::string generator("test_apidb_backend.cpp");
  routes route;
  auto factory = std::make_shared<empty_data_selection::factory>();

  test_request req;
  req.set_header("SCRIPT_URL", "/api/0.6/relation/165475/full");
  req.set_header("SCRIPT_URI",
		 "http://www.openstreetmap.org/api/0.6/relation/165475/full");
  req.set_header("HTTP_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_ACCEPT_ENCODING",
		 "gzip;q=1.0,deflate;q=0.6,identity;q=0.3");
  req.set_header("HTTP_ACCEPT", "*/*");
  req.set_header("HTTP_USER_AGENT", "OAuth gem v0.4.7");
  req.set_header("HTTP_AUTHORIZATION","Bearer " + token);
  req.set_header("HTTP_X_REQUEST_ID", "V-eaKX8AAQEAAF4UzHwAAAHt");
  req.set_header("HTTP_X_FORWARDED_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_X_FORWARDED_SERVER", "www.openstreetmap.org");
  req.set_header("HTTP_CONNECTION", "Keep-Alive");
  req.set_header("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
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
  req.set_header("REQUEST_METHOD", "GET");
  req.set_header("QUERY_STRING", "");
  req.set_header("REQUEST_URI", "/api/0.6/relation/165475/full");
  req.set_header("SCRIPT_NAME", "/api/0.6/relation/165475/full");

  process_request(req, limiter, generator, route, *factory, nullptr, &store);

  assert_equal<int>(expected_response_code, req.response_status(), "response status");
}


void test_oauth2_end_to_end(test_database &tdb) {

  // token 1yi2RI2WhIVMLoLaDLg0nrPJPU4WQSIX4Hh_jxfRRxI is stored in plain text in oauth_access_tokens table,
  // all others as sha256-hash value

  tdb.run_sql(R"(
    INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) 
    VALUES 
      (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true),
      (2, 'user_2@example.com', '', '2021-03-12T01:33:43Z', 'user_2', true);

    INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret, redirect_uri, scopes, confidential, created_at, updated_at) 
       VALUES (3, 'User', 1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8', '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c', 
              'http://demo.localhost:3000', 'write_api read_gpx', false, '2021-04-12 17:53:30', '2021-04-12 17:53:30');

    INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
       VALUES (67, 1, 3, '1yi2RI2WhIVMLoLaDLg0nrPJPU4WQSIX4Hh_jxfRRxI', NULL, NULL, NULL, '2021-04-14 19:38:21.991429', 'write_api', '');

    INSERT INTO public.oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes, previous_refresh_token)
       VALUES (72, 1, 3, '4ea5b956c8882db030a5a799cb45eb933bb6dd2f196a44f68167d96fbc8ec3f1', NULL, NULL, NULL, '2021-04-14 19:38:21.991429', 'read_prefs', '');
 
  )");

  auto store = tdb.get_oauth_store();

  // Test valid token -> HTTP 404 not found, due to unknown relation
  fetch_relation(tdb, *store, "1yi2RI2WhIVMLoLaDLg0nrPJPU4WQSIX4Hh_jxfRRxI", 404);

  // Test unknown token -> HTTP 401 Unauthorized
  fetch_relation(tdb, *store, "8JrrmoKSUtzBhmenUUQF27PVdQn2QY8YdRfosu3R-Dc", 401);

  // Test valid token, create empty changeset

  // missing write_api scope --> http::unauthorized ("You have not granted the modify map permission")
  create_changeset(tdb, *store, "hCXrz5B5fCBHusp0EuD2IGwYSxS8bkAnVw2_aLEdxig", 401);

  // includes write_api scope
  create_changeset(tdb, *store, "1yi2RI2WhIVMLoLaDLg0nrPJPU4WQSIX4Hh_jxfRRxI", 200);

}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(std::function<void(test_database&)>(
        &test_user_id_for_oauth2_token));

    tdb.run(std::function<void(test_database&)>(
        &test_oauth2_end_to_end));

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
