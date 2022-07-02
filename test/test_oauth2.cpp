#include "cgimap/oauth2.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <set>
#include <tuple>

#include <boost/date_time/posix_time/conversion.hpp>


#include <cassert>
#include <iostream>
#include <string>


#include <boost/date_time/posix_time/conversion.hpp>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include "cgimap/oauth2.hpp"
#include "test_request.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"

template<typename T>
std::ostream& operator<<(std::ostream& os, std::optional<T> const& opt)
{
  return opt ? os << opt.value() : os;
}

#define ANNOTATE_EXCEPTION(stmt)                \
  {                                             \
    try {                                       \
      stmt;                                     \
    } catch (const std::exception &e) {         \
      std::ostringstream ostr;                  \
      ostr << e.what() << ", during " #stmt ;   \
        throw std::runtime_error(ostr.str());   \
    }                                           \
  }

Transaction_Owner_Void::Transaction_Owner_Void() {}

pqxx::transaction_base& Transaction_Owner_Void::get_transaction() {
  throw std::runtime_error("get_transaction is not supported by Transaction_Owner_Void");
}

namespace {


template <typename T>
void assert_equal(const T &actual, const T &expected, const std::string& scope = "") {
  if (!(actual == expected)) {
    std::ostringstream ostr;
    if (!scope.empty())
      ostr << scope << ":";
    ostr << "Expected `" << expected << "', but got `" << actual << "'";
    throw std::runtime_error(ostr.str());
  }
}


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



void test_validate_bearer_token() {

  auto store = std::make_shared<test_oauth2>();

  {
    bool allow_api_write;
    test_request req;
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    assert_equal<std::optional<osm_user_id_t> >(res, {}, "Missing Header");
  }

  {
    bool allow_api_write;
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    assert_equal<std::optional<osm_user_id_t> >(res, {}, "Empty AUTH header");
  }

  // Test valid bearer token, no api_write
  {
    bool allow_api_write;
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Bearer 6GGXRGoDog0i6mRyrBonFmJORQhWZMhZH5WNWLd0qcs");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{1}, "Bearer token for user 1");
    assert_equal<bool>(allow_api_write, false, "Bearer token for user 1, allow_api_write");
  }

  // Test valid token including all allowed chars & padding chars - api_write allowed
  {
    bool allow_api_write;
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Bearer H4TeKX-zE_VLH.UT33_n6x__yZ8~BA~aQL+wfxQN/cADu7BMMA=====");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{2}, "Bearer token for user 2");
    assert_equal<bool>(allow_api_write, true, "Bearer token for user 2, allow_api_write");
  }


  // Test bearer token invalid format
  {
    bool allow_api_write;
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Bearer 6!#c23.-;<<>>");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    assert_equal<std::optional<osm_user_id_t> >(res, {}, "Invalid bearer format");
  }

  // Test invalid bearer token
  {
    bool allow_api_write;
    test_request req;
    try {
      req.set_header("HTTP_AUTHORIZATION","Bearer nFRBLFyNXPKY1fiTHAIfVsjQYkCD2KoRuH66upvueaQ");
      static_cast<void>(oauth2::validate_bearer_token(req, store.get(), allow_api_write));
      throw std::runtime_error("test_authenticate_user::001: Expected exception");
    } catch (http::unauthorized &e) {
      if (std::string(e.what()) != "invalid_token") {
        throw std::runtime_error("test_authenticate_user::001: Expected invalid_token");
      }
    }
  }

  // Test expired bearer token
  {
    bool allow_api_write;
    test_request req;
    try {
      req.set_header("HTTP_AUTHORIZATION","Bearer pwnMeCjSmIfQ9hXVYfAyFLFnE9VOADNvwGMKv4Ylaf0");
      static_cast<void>(oauth2::validate_bearer_token(req, store.get(), allow_api_write));
      throw std::runtime_error("test_authenticate_user::002: Expected exception");
    } catch (http::unauthorized &e) {
      if (std::string(e.what()) != "token_expired") {
        throw std::runtime_error("test_authenticate_user::002: Expected token_expired");
      }
    }
  }

  // Test revoked bearer token
  {
    bool allow_api_write;
    test_request req;
    try {
      req.set_header("HTTP_AUTHORIZATION","Bearer hCXrz5B5fCBHusp0EuD2IGwYSxS8bkAnVw2_aLEdxig");
      static_cast<void>(oauth2::validate_bearer_token(req, store.get(), allow_api_write));
      throw std::runtime_error("test_authenticate_user::003: Expected exception");
    } catch (http::unauthorized &e) {
      if (std::string(e.what()) != "token_revoked") {
        throw std::runtime_error("test_authenticate_user::003: Expected token_revoked");
      }
    }
  }

  // Test valid bearer token, no api_write
  {
    bool allow_api_write;
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Bearer 0LbSEAVj4jQhr-TfNaCUhn4JSAvXmXepNaL9aSAUsVQ");
    auto res = oauth2::validate_bearer_token(req, store.get(), allow_api_write);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{5}, "Bearer token for user 5");
    assert_equal<bool>(allow_api_write, false, "Bearer token for user 5, allow_api_write");
  }
}


int main() {
  try {
    ANNOTATE_EXCEPTION(test_validate_bearer_token());
  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}

