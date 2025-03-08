/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <optional>
#include <string>

#include "cgimap/oauth2.hpp"
#include "test_request.hpp"
#include "test_empty_selection.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

namespace {


class oauth2_test_data_selection : public empty_data_selection {
public:

  ~oauth2_test_data_selection() override = default;

  std::optional<osm_user_id_t> get_user_id_for_oauth2_token(const std::string &token_id,
                                                            bool& expired,
                                                            bool& revoked,
                                                            bool& allow_api_write) override {

    // Note: original token ids have been sha256 hashed, token_id hash values can be generated using
    // echo -n "6GGXRGoDog0i6mRyrBonFmJORQhWZMhZH5WNWLd0qcs" | sha256sum

#if HAVE_CRYPTOPP
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
#else
    // valid token - api write not allowed
    if (token_id == "6GGXRGoDog0i6mRyrBonFmJORQhWZMhZH5WNWLd0qcs") {
      expired = false;
      revoked = false;
      allow_api_write = false;
      return osm_user_id_t{1};

    // valid token including all allowed chars & padding chars - api_write allowed
    } else if (token_id == "H4TeKX-zE_VLH.UT33_n6x__yZ8~BA~aQL+wfxQN/cADu7BMMA=====") {
      expired = false;
      revoked = false;
      allow_api_write = true;
      return osm_user_id_t{2};

    // invalid token
    } else if (token_id == "nFRBLFyNXPKY1fiTHAIfVsjQYkCD2KoRuH66upvueaQ") {
      return {};

    // expired token for user 3
    } else if (token_id == "pwnMeCjSmIfQ9hXVYfAyFLFnE9VOADNvwGMKv4Ylaf0") {
      expired = true;
      revoked = false;
      allow_api_write = false;
      return osm_user_id_t{3};

    // revoked token for user 4
    } else if (token_id == "hCXrz5B5fCBHusp0EuD2IGwYSxS8bkAnVw2_aLEdxig") {
      expired = false;
      revoked = true;
      allow_api_write = false;
      return osm_user_id_t{4};
    }

#endif

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

  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t id) override {
    return {};
  }

  struct factory : public data_selection::factory {
    ~factory() override = default;
    std::unique_ptr<data_selection> make_selection(Transaction_Owner_Base&) const override {
      return std::make_unique<oauth2_test_data_selection>();
    }
    std::unique_ptr<Transaction_Owner_Base> get_default_transaction() override {
      return std::make_unique<Transaction_Owner_Void>();
    }
  };
};


} // anonymous namespace


TEST_CASE("test_validate_bearer_token", "[oauth2]") {

  bool allow_api_write;
  test_request req;

  auto factory = std::make_shared<oauth2_test_data_selection::factory>();
  auto txn_readonly = factory->get_default_transaction();
  auto sel = factory->make_selection(*txn_readonly);

  SECTION("Missing Header") {
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Missing Header") {
    req.set_header("HTTP_AUTHORIZATION","");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  //
  SECTION("Test valid bearer token, no api_write") {
    req.set_header("HTTP_AUTHORIZATION","Bearer 6GGXRGoDog0i6mRyrBonFmJORQhWZMhZH5WNWLd0qcs");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{1});
    CHECK(!allow_api_write);
  }

  //
  SECTION("Test valid token including all allowed chars & padding chars - api_write allowed") {
    req.set_header("HTTP_AUTHORIZATION","Bearer H4TeKX-zE_VLH.UT33_n6x__yZ8~BA~aQL+wfxQN/cADu7BMMA=====");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{2});
    CHECK(allow_api_write);
  }

  SECTION("Test bearer token invalid format (invalid chars)") {
    req.set_header("HTTP_AUTHORIZATION","Bearer 6!#c23.-;<<>>");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Test bearer token invalid format (extra space after bearer)") {
    req.set_header("HTTP_AUTHORIZATION","Bearer  abc");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Test bearer token invalid format (lowercase Bearer)") {
    req.set_header("HTTP_AUTHORIZATION","bearer abc");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Test bearer token invalid format (trailing space after token)") {
    req.set_header("HTTP_AUTHORIZATION","Bearer abcdefghijklm ");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Test bearer token invalid format (missing tokan)") {
    req.set_header("HTTP_AUTHORIZATION","Bearer ");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Test invalid bearer token") {
    req.set_header("HTTP_AUTHORIZATION","Bearer nFRBLFyNXPKY1fiTHAIfVsjQYkCD2KoRuH66upvueaQ");
    REQUIRE_THROWS_MATCHES(static_cast<void>(oauth2::validate_bearer_token(req, *sel, allow_api_write)), http::unauthorized,
        Catch::Message("invalid_token"));
  }

  SECTION("Test expired bearer token") {
    req.set_header("HTTP_AUTHORIZATION","Bearer pwnMeCjSmIfQ9hXVYfAyFLFnE9VOADNvwGMKv4Ylaf0");
    REQUIRE_THROWS_MATCHES(static_cast<void>(oauth2::validate_bearer_token(req, *sel, allow_api_write)),
                      http::unauthorized,
                      Catch::Message("token_expired"));
  }

  SECTION("Test revoked bearer token") {
    req.set_header("HTTP_AUTHORIZATION","Bearer hCXrz5B5fCBHusp0EuD2IGwYSxS8bkAnVw2_aLEdxig");
    REQUIRE_THROWS_MATCHES(static_cast<void>(oauth2::validate_bearer_token(req, *sel, allow_api_write)),
                      http::unauthorized,
                      Catch::Message("token_revoked"));
  }

  SECTION("Test valid bearer token, no api_write") {
    req.set_header("HTTP_AUTHORIZATION","Bearer 0LbSEAVj4jQhr-TfNaCUhn4JSAvXmXepNaL9aSAUsVQ");
    auto res = oauth2::validate_bearer_token(req, *sel, allow_api_write);
    CHECK(res == std::optional<osm_user_id_t>{5});
    CHECK(!allow_api_write);
  }
}
