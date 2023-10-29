#include <iostream>
#include <stdexcept>
#include <fmt/core.h>
#include <boost/program_options.hpp>

#include <sys/time.h>
#include <cstdio>

#include "cgimap/config.hpp"
#include "cgimap/time.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/options.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"
#include "test_request.hpp"

using roles_t = std::set<osm_user_role_t>;

namespace {

class global_settings_test_no_oauth1 : public global_settings_default {

public:

  bool get_oauth_10_support() const override {
    return false;
  }
};


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

}


template <> struct fmt::formatter<roles_t> {
  template <typename FormatContext>
  auto format(const roles_t& r, FormatContext& ctx) -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    std::ostringstream ostr;
    ostr << r;
    return format_to(ctx.out(), "{}", ostr.str());
  }
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
};

namespace {


template<typename T>
std::ostream& operator<<(std::ostream& os, std::optional<T> const& opt)
{
  return opt ? os << opt.value() : os;
}

template <typename T>
void assert_equal(const T& a, const T&b, const std::string &message) {
  if (a != b) {
    throw std::runtime_error(fmt::format("Expecting {} to be equal, but {} != {}", message, a, b));
  }
}

void test_nonce_store(test_database &tdb) {
  tdb.run_sql("");
  auto store = tdb.get_oauth_store();

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
  auto store = tdb.get_oauth_store();

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

void test_allow_write_api(test_database &tdb) {
  tdb.run_sql(
    "INSERT INTO users (id, email, pass_crypt, creation_time, display_name, data_public) "
    "VALUES "
    "  (11, 'user_11@example.com', '', '2013-11-14T02:10:00Z', 'user_11', true); "

    "INSERT INTO client_applications "
    "  (id, name, user_id, allow_read_prefs, allow_write_api, key, secret) "
    "VALUES "
    "  (111, 'test_client_application111', 11, true, true, "
    "   'x3tHSMbotPe5fBlItMbg', '1NZRJ0u2o7OilPDe60nfZsKJTC7RUZPrNfYwGBjATw'); "

    "INSERT INTO oauth_tokens "
    "  (id, user_id, client_application_id, allow_read_prefs, allow_write_api, token, secret, "
    "   created_at, authorized_at, invalidated_at) "
    "VALUES "
    // write api permitted
    "  (11, 11, 111, true, true,"
    "   'AfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA', "
    "   'fFCKdXsLxeHPlgrIPr5fZSpXKnDuLw0GvJTzeE99', "
    "   '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z', NULL), "
    // write api not permitted
    "  (12, 11, 111, true, false,"
    "   'ApNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC', "
    "   'NZskvUUYlOuCsPKuMbSTz5eMpVJVI3LsyW11Z2Uq', "
    "   '2016-07-11T19:12:00Z', '2016-07-11T19:12:00Z', NULL); "
    "");
  auto store = tdb.get_oauth_store();

  assert_equal<bool>(
    true, store->allow_write_api("AfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA"),
    "valid token and api write permitted allows writing API");

  assert_equal<bool>(
    false, store->allow_write_api("ApNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC"),
    "valid token and api write not permitted does not allow writing API");
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
  auto store = tdb.get_oauth_store();

  assert_equal<osm_user_id_t >(
    1, store->get_user_id_for_token("OfkxM4sSeyXjzgDTIOaJxcutsnqBoalr842NHOrA").value_or(0),
    "valid token belongs to user 1");

  assert_equal<osm_user_id_t >(
    1, store->get_user_id_for_token("wpNsXPhrgWl4ELPjPbhfwjjSbNk9npsKoNrMGFlC").value_or(0),
    "non-authorized token belongs to user 1");

  assert_equal<osm_user_id_t>(
    1, store->get_user_id_for_token("Rzcm5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf").value_or(0),
    "invalid token belongs to user 1");

  assert_equal<bool>(
    false,
    store->get_user_id_for_token("____5aDiDgqgub8j96MfDaYyAc4cRwI9CmZB7HBf").has_value(),
    "non-existent token does not belong to anyone");
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
  bool is_user_active(const osm_user_id_t id) override { return (id != 1000); }

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

  std::tuple<bool, int> check(const std::string &key, bool moderator) {
    m_keys_seen.insert(key);
    return std::make_tuple(false, 0);
  }

  void update(const std::string &key, int bytes, bool moderator) {
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
  auto store = tdb.get_oauth_store();

  recording_rate_limiter limiter;
  std::string generator("test_apidb_backend.cpp");
  routes route;
  auto factory = std::make_unique<empty_data_selection::factory>();

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

  assert_equal<std::string>(
    std::string("ewKFprItE5uaDHKFu3IVzuEHbno="),
    oauth::detail::hashed_signature(req, *store).value_or(""),
    "hashed signatures");

  process_request(req, limiter, generator, route, *factory, nullptr, store.get());

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
  auto store = tdb.get_oauth_store();

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

void test_oauth_disabled_by_global_config(test_database &tdb) {

  auto test_settings = std::unique_ptr<global_settings_test_no_oauth1>(new global_settings_test_no_oauth1());
  global_settings::set_configuration(std::move(test_settings));

  // TODO

}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(std::function<void(test_database&)>(
        &test_nonce_store));

    tdb.run(std::function<void(test_database&)>(
        &test_allow_read_api));

    tdb.run(std::function<void(test_database&)>(
        &test_allow_write_api));

    tdb.run(std::function<void(test_database&)>(
        &test_get_user_id_for_token));

    tdb.run(std::function<void(test_database&)>(
        &test_oauth_end_to_end));

    tdb.run(std::function<void(test_database&)>(
        &test_oauth_get_roles_for_user));

    tdb.run(std::function<void(test_database&)>(
        &test_oauth_disabled_by_global_config));

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
