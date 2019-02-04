#include <cassert>
#include <iostream>
#include <string>

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <sstream>
#include <set>

#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include "cgimap/basicauth.hpp"
#include "test_request.hpp"

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

namespace {

void assert_true(bool value) {
  if (!value) {
    throw std::runtime_error("Test failed: Expecting true, but got false.");
  }
}

template <typename T>
void assert_equal(const T &actual, const T &expected) {
  if (!(actual == expected)) {
    std::ostringstream ostr;
    ostr << "Expected `" << expected << "', but got `" << actual << "'";
    throw std::runtime_error(ostr.str());
  }
}

template <typename T>
void assert_equal(const T &actual, const T &expected, const std::string& scope) {
  if (!(actual == expected)) {
    std::ostringstream ostr;
    ostr << scope << ": Expected `" << expected << "', but got `" << actual << "'";
    throw std::runtime_error(ostr.str());
  }
}



class basicauth_test_data_selection
  : public data_selection {
public:

  virtual ~basicauth_test_data_selection() = default;

  void write_nodes(output_formatter &formatter) {}
  void write_ways(output_formatter &formatter) {}
  void write_relations(output_formatter &formatter) {}
  void write_changesets(output_formatter &formatter,
                        const std::chrono::system_clock::time_point &now) {}

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

  bool get_user_id_pass(const std::string& display_name, osm_user_id_t & user_id,
				std::string & pass_crypt, std::string & pass_salt) {

    if (display_name == "demo") {
	user_id = 4711;
        pass_crypt = "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=";
        pass_salt = "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=";
	return true;
    }

    return false;
  }

  struct factory
    : public data_selection::factory {
    virtual ~factory() = default;
    virtual std::shared_ptr<data_selection> make_selection() {
      return std::make_shared<basicauth_test_data_selection>();
    }
  };
};

} // anonymous namespace



void test_password_hash() {

  PasswordHash pwd_hash;

  // test_md5_without_salt
  assert_equal<bool>(pwd_hash.check("5f4dcc3b5aa765d61d8327deb882cf99", "",
                             "password"), true);

  assert_equal<bool>(pwd_hash.check("5f4dcc3b5aa765d61d8327deb882cf99", "", "wrong"), false);

  // test_md5_with_salt
  assert_equal<bool>(pwd_hash.check("67a1e09bb1f83f5007dc119c14d663aa", "salt",
                             "password"), true);
  assert_equal<bool>(pwd_hash.check("67a1e09bb1f83f5007dc119c14d663aa", "salt",
                             "wrong"), false);
  assert_equal<bool>(pwd_hash.check("67a1e09bb1f83f5007dc119c14d663aa", "wrong",
                             "password"), false);

  // test_pbkdf2_1000_32_sha512
  assert_equal<bool>(pwd_hash.check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "password"), true);

  assert_equal<bool>(pwd_hash.check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "wrong"), false);

  assert_equal<bool>(pwd_hash.check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gwrongtoNzm/CNKe4cf7bPKwdUNrk=",
             "password"), false);

  // test_pbkdf2_10000_32_sha512
  assert_equal<bool>(pwd_hash.check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password"), true);

  assert_equal<bool>(pwd_hash.check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "wrong"), false);

  assert_equal<bool>(pwd_hash.check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtMwronguvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password"), false);

}

void test_authenticate_user() {

  std::shared_ptr<data_selection::factory> factory =
    std::make_shared<basicauth_test_data_selection::factory>();

  auto sel = factory->make_selection();

  {
    test_request req;
    auto res = basicauth::authenticate_user(req, sel);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{}, "Missing Header");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","");
    auto res = basicauth::authenticate_user(req, sel);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{}, "Empty AUTH header");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ");
    auto res = basicauth::authenticate_user(req, sel);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{}, "Empty AUTH header");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbw==");
    auto res = basicauth::authenticate_user(req, sel);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{}, "User without password");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzo=");
    auto res = basicauth::authenticate_user(req, sel);
    assert_equal<boost::optional<osm_user_id_t> >(res,boost::optional<osm_user_id_t>{}, "User and colon without password");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzpwYXNzd29yZA==");
    auto res = basicauth::authenticate_user(req, sel);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{4711}, "Known user with correct password");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic TotalCrapData==");
    auto res = basicauth::authenticate_user(req, sel);
    assert_equal<boost::optional<osm_user_id_t> >(res, boost::optional<osm_user_id_t>{}, "Crap data");
  }

  // Test with known user and incorrect password
  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzppbmNvcnJlY3Q=");
    try {
      auto res = basicauth::authenticate_user(req, sel);
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
      auto res = basicauth::authenticate_user(req, sel);
      throw std::runtime_error("Unknown user / incorrect password: expected http unauthorized exception");

    } catch (http::exception &e) {
      if (e.code() != 401)
        throw std::runtime_error(
            "Unknown user / incorrect password: Expected HTTP 401");
    }
  }

}


int main() {
  try {
    ANNOTATE_EXCEPTION(test_password_hash());
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



