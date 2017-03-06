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

std::ostream &operator<<(
  std::ostream &out, const std::set<osm_user_role_t> &roles) {

  out << "{";
  bool first = true;
  for (osm_user_role_t r : roles) {
    if (first) { first = false; } else { out << ", "; }
    if (r == osm_user_role_t::moderator) {
      out << "moderator";
    } else if (r == osm_user_role_t::administrator) {
      out << "administrator";
    }
  }
  out << "}";
  return out;
}

template <typename T>
void assert_equal(const T& a, const T&b, const std::string &message) {
  if (a != b) {
    std::ostringstream out;
    out << "Expecting " << message << " to be equal, but "
        << a << " != " << b;
    throw std::runtime_error(out.str());
  }
}

void test_nonce_store(test_database &tdb) {
  tdb.run_sql("");
  boost::shared_ptr<oauth::store> store = tdb.get_oauth_store();

  // can use a nonce
  assert_equal<bool>(true, store->use_nonce("abcdef", 0), "first use of nonce");

  // can't use it twice
  assert_equal<bool>(false, !store->use_nonce("abcdef", 0),
                     "second use of the same nonce");

  // can use the same nonce with a different timestamp
  assert_equal<bool>(true, store->use_nonce("abcdef", 1),
                     "use of nonce with a different timestamp");

  // or the same timestamp with a different nonce
  assert_equal<bool>(true, store->use_nonce("abcdeg", 0),
                     "use of nonce with a different nonce string");
}

void test_allow_read_api(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO client_applications "
    "  (id, name, user_id, allow_read_prefs, key, secret) "
    "VALUES "
    "  (1, 'test_client_application', 1, true, "
    "   'x3tHSMbotPe5fBlItMbg', '1NZRJ0u2o7OilPDe60nfZsKJTC7RUZPrNfYwGBjATw'); "

    "INSERT INTO oauth_tokens "
    "  (id, user_id, client_application_id, allow_read_prefs, token, secret, "
    "   created_at, authorized_at, invalidated_at) "
    "VALUES "
    // valid key
    "  (1, 1, 1, true, "
    "   'OfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA', "
    "   'fFCKdXsLxeHPlgrIPr5fZSpXKnDuLw0GvJTzeE99', "
    "   '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z', NULL), "
    // not authorized
    "  (2, 1, 1, true, "
    "   'wpNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC', "
    "   'NZskvUUYlOuCsPKuMbSTz5eMpVJVI3LsyW11Z2Uq', "
    "   '2016-07-11T19:12:00Z', NULL, NULL), "
    // invalidated
    "  (3, 1, 1, true, "
    "   'Rzcm5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf', "
    "   '2UxsEFziZGv64hdWN3Qa90Vb6v1aovVxaTTQIn1D', "
    "   '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z'); "
    "");
  boost::shared_ptr<oauth::store> store = tdb.get_oauth_store();

  assert_equal<bool>(
    true, store->allow_read_api("OfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA"),
    "valid token allows reading API");

  assert_equal<bool>(
    false, store->allow_read_api("wpNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC"),
    "non-authorized token does not allow reading API");

  assert_equal<bool>(
    false, store->allow_read_api("Rzcm5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf"),
    "invalid token does not allow reading API");
}

void test_get_user_id_for_token(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO client_applications "
    "  (id, name, user_id, allow_read_prefs, key, secret) "
    "VALUES "
    "  (1, 'test_client_application', 1, true, "
    "   'x3tHSMbotPe5fBlItMbg', '1NZRJ0u2o7OilPDe60nfZsKJTC7RUZPrNfYwGBjATw'); "

    "INSERT INTO oauth_tokens "
    "  (id, user_id, client_application_id, allow_read_prefs, token, secret, "
    "   created_at, authorized_at, invalidated_at) "
    "VALUES "
    // valid key
    "  (1, 1, 1, true, "
    "   'OfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA', "
    "   'fFCKdXsLxeHPlgrIPr5fZSpXKnDuLw0GvJTzeE99', "
    "   '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z', NULL), "
    // not authorized
    "  (2, 1, 1, true, "
    "   'wpNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC', "
    "   'NZskvUUYlOuCsPKuMbSTz5eMpVJVI3LsyW11Z2Uq', "
    "   '2016-07-11T19:12:00Z', NULL, NULL), "
    // invalidated
    "  (3, 1, 1, true, "
    "   'Rzcm5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf', "
    "   '2UxsEFziZGv64hdWN3Qa90Vb6v1aovVxaTTQIn1D', "
    "   '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z'); "
    "");
  boost::shared_ptr<oauth::store> store = tdb.get_oauth_store();

  assert_equal<boost::optional<osm_user_id_t> >(
    1, store->get_user_id_for_token("OfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA"),
    "valid token belongs to user 1");

  assert_equal<boost::optional<osm_user_id_t> >(
    1, store->get_user_id_for_token("wpNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC"),
    "non-authorized token belongs to user 1");

  assert_equal<boost::optional<osm_user_id_t> >(
    1, store->get_user_id_for_token("Rzcm5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf"),
    "invalid token belongs to user 1");

  assert_equal<boost::optional<osm_user_id_t> >(
    boost::none,
    store->get_user_id_for_token("____5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf"),
    "non-existent token does not belong to anyone");
}

class empty_data_selection
  : public data_selection {
public:

  virtual ~empty_data_selection() {}

  void write_nodes(output_formatter &formatter) {}
  void write_ways(output_formatter &formatter) {}
  void write_relations(output_formatter &formatter) {}
  void write_changesets(output_formatter &formatter,
                        const boost::posix_time::ptime &now) {}

  visibility_t check_node_visibility(osm_nwr_id_t id) {}
  visibility_t check_way_visibility(osm_nwr_id_t id) {}
  visibility_t check_relation_visibility(osm_nwr_id_t id) {}

  int select_nodes(const std::vector<osm_nwr_id_t> &) { return 0; }
  int select_ways(const std::vector<osm_nwr_id_t> &) { return 0; }
  int select_relations(const std::vector<osm_nwr_id_t> &) { return 0; }
  int select_nodes_from_bbox(const bbox &bounds, int max_nodes) { return 0; }
  void select_nodes_from_relations() {}
  void select_ways_from_nodes() {}
  void select_ways_from_relations() {}
  void select_relations_from_ways() {}
  void select_nodes_from_way_nodes() {}
  void select_relations_from_nodes() {}
  void select_relations_from_relations() {}
  void select_relations_members_of_relations() {}
  bool supports_changesets() { return false; }
  int select_changesets(const std::vector<osm_changeset_id_t> &) { return 0; }
  void select_changeset_discussions() {}

  struct factory
    : public data_selection::factory {
    virtual ~factory() {}
    virtual boost::shared_ptr<data_selection> make_selection() {
      return boost::make_shared<empty_data_selection>();
    }
  };
};

struct recording_rate_limiter
  : public rate_limiter {
  ~recording_rate_limiter() {}

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

void test_oauth_end_to_end(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2013-11-14T02:10:00Z', 'user_1', true); "

    "INSERT INTO client_applications "
    "  (id, name, user_id, allow_read_prefs, key, secret) "
    "VALUES "
    "  (1, 'test_client_application', 1, true, "
    "   'x3tHSMbotPe5fBlItMbg', '1NZRJ0u2o7OilPDe60nfZsKJTC7RUZPrNfYwGBjATw'); "

    "INSERT INTO oauth_tokens "
    "  (id, user_id, client_application_id, allow_read_prefs, token, secret, "
    "   created_at, authorized_at, invalidated_at) "
    "VALUES "
    "  (4, 1, 1, true, "
    "   '15zpwgGjdjBu1DD65X7kcHzaWqfQpvqmMtqa3ZIO', "
    "   'H3Vb9Kgf4LpTyVlft5xsI9MwzknQsTu6CkHE0qK3', "
    "   '2016-10-07T00:00:00Z', '2016-10-07T00:00:00Z', NULL); "
    "");
  boost::shared_ptr<oauth::store> store = tdb.get_oauth_store();

  recording_rate_limiter limiter;
  std::string generator("test_apidb_backend.cpp");
  routes route;
  boost::shared_ptr<data_selection::factory> factory =
    boost::make_shared<empty_data_selection::factory>();

  test_request req;
  req.set_header("SCRIPT_URL", "/api/0.6/relation/165475/full");
  req.set_header("SCRIPT_URI",
                 "http://www.openstreetmap.org/api/0.6/relation/165475/full");
  req.set_header("HTTP_HOST", "www.openstreetmap.org");
  req.set_header("HTTP_ACCEPT_ENCODING",
                 "gzip;q=1.0,deflate;q=0.6,identity;q=0.3");
  req.set_header("HTTP_ACCEPT", "*/*");
  req.set_header("HTTP_USER_AGENT", "OAuth gem v0.4.7");
  req.set_header("HTTP_AUTHORIZATION",
                 "OAuth oauth_consumer_key=\"x3tHSMbotPe5fBlItMbg\", "
                 "oauth_nonce=\"dvu3eTk8i1uvj8zQ8Wef91UF6ngQdlTA3xQ2vEf7xU\", "
                 "oauth_signature=\"ewKFprItE5uaDHKFu3IVzuEHbno%3D\", "
                 "oauth_signature_method=\"HMAC-SHA1\", "
                 "oauth_timestamp=\"1475844649\", "
                 "oauth_token=\"15zpwgGjdjBu1DD65X7kcHzaWqfQpvqmMtqa3ZIO\", "
                 "oauth_version=\"1.0\"");
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

  assert_equal<boost::optional<std::string> >(
    std::string("ewKFprItE5uaDHKFu3IVzuEHbno="),
    oauth::detail::hashed_signature(req, *store),
    "hashed signatures");

  process_request(req, limiter, generator, route, factory, store);

  assert_equal<int>(404, req.response_status(), "response status");
  assert_equal<bool>(false, limiter.saw_key("addr:127.0.0.1"),
                     "saw addr:127.0.0.1 as a rate limit key");
  assert_equal<bool>(true, limiter.saw_key("user:1"),
                     "saw user:1 as a rate limit key");
}

void test_oauth_get_roles_for_user(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (1, 'user_1@example.com', '', '2017-02-20T11:41:00Z', 'user_1', true), "
    "  (2, 'user_2@example.com', '', '2017-02-20T11:41:00Z', 'user_2', true), "
    "  (3, 'user_3@example.com', '', '2017-02-20T11:41:00Z', 'user_3', true); "

    "INSERT INTO user_roles (id, user_id, role, granter_id) "
    "VALUES "
    "  (1, 1, 'administrator', 1), "
    "  (2, 1, 'moderator', 1), "
    "  (3, 2, 'moderator', 1);"
    "");
  boost::shared_ptr<oauth::store> store = tdb.get_oauth_store();

  typedef std::set<osm_user_role_t> roles_t;

  // user 3 has no roles -> should return empty set
  assert_equal<roles_t>(roles_t(), store->get_roles_for_user(3),
    "roles for normal user");

  // user 2 is a moderator
  assert_equal<roles_t>(
    roles_t({osm_user_role_t::moderator}),
    store->get_roles_for_user(2),
    "roles for moderator user");

  // user 1 is an administrator and a moderator
  assert_equal<roles_t>(
    roles_t({osm_user_role_t::moderator, osm_user_role_t::administrator}),
    store->get_roles_for_user(1),
    "roles for admin+moderator user");
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(boost::function<void(test_database&)>(
        &test_nonce_store));

    tdb.run(boost::function<void(test_database&)>(
        &test_allow_read_api));

    tdb.run(boost::function<void(test_database&)>(
        &test_get_user_id_for_token));

    tdb.run(boost::function<void(test_database&)>(
        &test_oauth_end_to_end));

    tdb.run(boost::function<void(test_database&)>(
        &test_oauth_get_roles_for_user));

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
