
#include <stdexcept>
#include <optional>
#include <string>

#include "cgimap/oauth2.hpp"
#include "test_request.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>


Transaction_Owner_Void::Transaction_Owner_Void() {}

pqxx::transaction_base& Transaction_Owner_Void::get_transaction() {
  throw std::runtime_error("get_transaction is not supported by Transaction_Owner_Void");
}

std::set<std::string>& Transaction_Owner_Void::get_prep_stmt() {
  throw std::runtime_error ("get_prep_stmt is not supported by Transaction_Owner_Void");
}

namespace {

struct test_oauth2
  : public oauth::store {

  test_oauth2() = default;

  virtual ~test_oauth2() = default;


  bool allow_read_api(const std::string &token_id) {
    // everyone can read the api
    return true;
  }

  bool allow_write_api(const std::string &token_id) {
    // everyone can write the api for the moment
    return true;
  }

  std::optional<osm_user_id_t> get_user_id_for_token(const std::string &token_id) {
    return {};
  }

  std::optional<osm_user_id_t> get_user_id_for_oauth2_token(const std::string &token_id, bool& expired, bool& revoked, bool& allow_api_write) {

    // Note: original token ids have been sha256 hashed, token_id hash values can be generated using
    // echo -n "6GGXRGoDog0i6mRyrBonFmJORQhWZMhZH5WNWLd0qcs" | sha256sum

    // valid token - api write not allowed
    if (token_id == "deb2029737bcfaaf9e937aea6b5d585a1bf93be9d21672d0f98c479c52592130") { // "6GGXRGoDog0i6mRyrBonFmJORQhWZMhZH5WNWLd0qcs"
      expired = false;
      revoked = false;
      allow_api_write = false;
      return osm_user_id_t{1};

    // valid token including all allowed chars & padding chars - api_write allowed
    } else if (token_id == "b708e84f9f2135b2ebd4a87529a6d0da976939e37958ac63f5790d8e3f4eb7db") { // "H4TeKX-zE_VLH.UT33_n6x__yZ8~BA~aQL+wfxQN/cADu7BMMA====="
      expired = false;
      revoked = false;
      allow_api_write = true;
      return osm_user_id_t{2};

    // invalid token
    } else if (token_id == "f3565d87316a9f5eb134f3d129e76fc82798d4ede12b59f4b3f2094aa61b0ce2") { // "nFRBLFyNXPKY1fiTHAIfVsjQYkCD2KoRuH66upvueaQ"
      return {};

    // expired token for user 3
    } else if (token_id == "42ad2fc9589b134e57cecab938873490aebfb0c7c6430f3c62485a693c6be62d") { // "pwnMeCjSmIfQ9hXVYfAyFLFnE9VOADNvwGMKv4Ylaf0"
      expired = true;
      revoked = false;
      allow_api_write = false;
      return osm_user_id_t{3};

    // revoked token for user 4
    } else if (token_id == "4ea5b956c8882db030a5a799cb45eb933bb6dd2f196a44f68167d96fbc8ec3f1") { // "hCXrz5B5fCBHusp0EuD2IGwYSxS8bkAnVw2_aLEdxig"
      expired = false;
      revoked = true;
      allow_api_write = false;
      return osm_user_id_t{4};
    }

    // valid token (plain) - api write not allowed
    if (token_id == "0LbSEAVj4jQhr-TfNaCUhn4JSAvXmXepNaL9aSAUsVQ") {
      expired = false;
      revoked = false;
      allow_api_write = false;
      return osm_user_id_t{5};
    }

    // default: invalid token
    return {};

  }

  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t id) {
    return {};
  }

  std::optional<std::string> consumer_secret(const std::string &consumer_key) { return {}; }
  std::optional<std::string> token_secret(const std::string &token_id) { return {}; }
  bool use_nonce(const std::string &nonce, uint64_t timestamp) { return true; }
};

} // anonymous namespace


TEST_CASE("test_validate_bearer_token", "[oauth2]") {

  bool allow_api_write;
  test_request req;
  auto store = std::make_shared<test_oauth2>();

  SECTION("Missing Header") {
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Missing Header") {
    req.set_header("HTTP_AUTHORIZATION","");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  //
  SECTION("Test valid bearer token, no api_write") {
    req.set_header("HTTP_AUTHORIZATION","Bearer 6GGXRGoDog0i6mRyrBonFmJORQhWZMhZH5WNWLd0qcs");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{1});
    CHECK(!allow_api_write);
  }

  //
  SECTION("Test valid token including all allowed chars & padding chars - api_write allowed") {
    req.set_header("HTTP_AUTHORIZATION","Bearer H4TeKX-zE_VLH.UT33_n6x__yZ8~BA~aQL+wfxQN/cADu7BMMA=====");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{2});
    CHECK(allow_api_write);
  }

  SECTION("Test bearer token invalid format") {
    req.set_header("HTTP_AUTHORIZATION","Bearer 6!#c23.-;<<>>");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Test invalid bearer token") {

    req.set_header("HTTP_AUTHORIZATION","Bearer nFRBLFyNXPKY1fiTHAIfVsjQYkCD2KoRuH66upvueaQ");
    REQUIRE_THROWS_MATCHES(static_cast<void>(oauth2::validate_bearer_token(req, store.get(), allow_api_write)), http::unauthorized,
        Catch::Message("invalid_token"));
  }

  SECTION("Test expired bearer token") {
    req.set_header("HTTP_AUTHORIZATION","Bearer pwnMeCjSmIfQ9hXVYfAyFLFnE9VOADNvwGMKv4Ylaf0");
    REQUIRE_THROWS_MATCHES(static_cast<void>(oauth2::validate_bearer_token(req, store.get(), allow_api_write)),
                      http::unauthorized,
                      Catch::Message("token_expired"));
  }

  SECTION("Test revoked bearer token") {
    req.set_header("HTTP_AUTHORIZATION","Bearer hCXrz5B5fCBHusp0EuD2IGwYSxS8bkAnVw2_aLEdxig");
    REQUIRE_THROWS_MATCHES(static_cast<void>(oauth2::validate_bearer_token(req, store.get(), allow_api_write)),
                      http::unauthorized,
                      Catch::Message("token_revoked"));
  }

  SECTION("Test valid bearer token, no api_write") {
    req.set_header("HTTP_AUTHORIZATION","Bearer 0LbSEAVj4jQhr-TfNaCUhn4JSAvXmXepNaL9aSAUsVQ");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{5});
    CHECK(!allow_api_write);
  }
}
