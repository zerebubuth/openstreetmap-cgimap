#include "cgimap/oauth2.hpp"

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>
#include <set>
#include <tuple>

#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

#include <cassert>
#include <iostream>
#include <string>


#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include "cgimap/oauth2.hpp"
#include "test_request.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"

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

Transaction_Owner_Void::Transaction_Owner_Void() {};

pqxx::transaction_base& Transaction_Owner_Void::get_transaction() {
  throw std::runtime_error("get_transaction is not supported by Transaction_Owner_Void");
}

namespace {

void assert_true(bool value) {
  if (!value) {
    throw std::runtime_error("Test failed: Expecting true, but got false.");
  }
}


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

  boost::optional<osm_user_id_t> get_user_id_for_token(const std::string &token_id) {
    return {};
  }

  boost::optional<osm_user_id_t> get_user_id_for_oauth2_token(const std::string &token_id, bool& expired, bool& revoked, bool& allow_api_write) {
    expired = false;
    revoked = false;
    allow_api_write = false;
    return boost::none;
  }

  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t id) {
    return {};
  }

  boost::optional<std::string> consumer_secret(const std::string &consumer_key) { return {}; }
  boost::optional<std::string> token_secret(const std::string &token_id) { return {}; }
  bool use_nonce(const std::string &nonce, uint64_t timestamp) { return true; }
};

} // anonymous namespace



void test_authenticate_user() {

  std::shared_ptr<oauth::store> store = std::make_shared<test_oauth2>();

  {
    bool allow_api_write;
    test_request req;
    auto res = oauth2::validate_bearer_token(req, store, allow_api_write);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{}, "Missing Header");
  }

  {
    bool allow_api_write;
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","");
    auto res = oauth2::validate_bearer_token(req, store, allow_api_write);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{}, "Empty AUTH header");
  }
/*
  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzpwYXNzd29yZA==");
    auto res = oauth2::validate_bearer_token(req, store);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{4711}, "Known user with correct password");
  }

  // Test with known user and incorrect password
  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzppbmNvcnJlY3Q=");
    try {
      auto res = oauth2::validate_bearer_token(req, store);
      throw std::runtime_error("Known user, incorrect password: expected http unauthorized exception");

    } catch (http::exception &e) {
      if (e.code() != 401)
        throw std::runtime_error(
            "Known user / incorrect password: Expected HTTP 401");
    }
  }

  // Test with unknown user and incorrect password
  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzI6aW5jb3JyZWN0");
    try {
      auto res = oauth2::validate_bearer_token(req, store);
      throw std::runtime_error("Unknown user / incorrect password: expected http unauthorized exception");

    } catch (http::exception &e) {
      if (e.code() != 401)
        throw std::runtime_error(
            "Unknown user / incorrect password: Expected HTTP 401");
    }
  }
*/
}


int main() {
  try {
    ANNOTATE_EXCEPTION(test_authenticate_user());
  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}

