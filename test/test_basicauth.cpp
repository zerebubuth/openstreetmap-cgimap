#include <cassert>
#include <iostream>
#include <string>

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <set>

#include <boost/date_time/posix_time/conversion.hpp>

#include "cgimap/basicauth.hpp"
#include "cgimap/options.hpp"
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
  bool get_user_id_pass(const std::string& user_name, osm_user_id_t & user_id,
				std::string & pass_crypt, std::string & pass_salt) override {

    if (user_name == "demo") {
	user_id = 4711;
        pass_crypt = "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=";
        pass_salt = "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=";
	return true;
    }

    if (user_name == "argon2") {
	user_id = 4712;
        pass_crypt = "$argon2id$v=19$m=65536,t=1,p=1$KXGHWfWMf5H5kY4uU3ua8A$YroVvX6cpJpljTio62k19C6UpuIPtW7me2sxyU2dyYg";
        pass_salt = "";
	return true;
    }

    return false;
  }

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
      return std::make_unique<basicauth_test_data_selection>();
    }
    virtual std::unique_ptr<Transaction_Owner_Base> get_default_transaction() {
      return std::unique_ptr<Transaction_Owner_Void>(new Transaction_Owner_Void());
    }
  };
};

class global_settings_test_no_basic_auth : public global_settings_default {

public:

  bool get_basic_auth_support() const override {
    return false;
  }
};

} // anonymous namespace



void test_password_hash() {

  // test_md5_without_salt
  assert_equal<bool>(PasswordHash::check("5f4dcc3b5aa765d61d8327deb882cf99", "",
                             "password"), true);

  assert_equal<bool>(PasswordHash::check("5f4dcc3b5aa765d61d8327deb882cf99", "", "wrong"), false);

  // test_md5_with_salt
  assert_equal<bool>(PasswordHash::check("67a1e09bb1f83f5007dc119c14d663aa", "salt",
                             "password"), true);
  assert_equal<bool>(PasswordHash::check("67a1e09bb1f83f5007dc119c14d663aa", "salt",
                             "wrong"), false);
  assert_equal<bool>(PasswordHash::check("67a1e09bb1f83f5007dc119c14d663aa", "wrong",
                             "password"), false);

  // test_pbkdf2_1000_32_sha512
  assert_equal<bool>(PasswordHash::check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "password"), true);

  assert_equal<bool>(PasswordHash::check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "wrong"), false);

  assert_equal<bool>(PasswordHash::check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gwrongtoNzm/CNKe4cf7bPKwdUNrk=",
             "password"), false);

  // test_pbkdf2_10000_32_sha512
  assert_equal<bool>(PasswordHash::check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password"), true);

  assert_equal<bool>(PasswordHash::check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "wrong"), false);

  assert_equal<bool>(PasswordHash::check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtMwronguvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password"), false);

  // test argon2
  assert_equal<bool>(PasswordHash::check(
             "$argon2id$v=19$m=65536,t=1,p=1$KXGHWfWMf5H5kY4uU3ua8A$YroVvX6cpJpljTio62k19C6UpuIPtW7me2sxyU2dyYg",
             "",
             "password"), true);

  assert_equal<bool>(PasswordHash::check(
             "$argon2id$v=19$m=65536,t=1,p=1$KXGHWfWMf5H5kY4uU3ua8A$YroVvX6cpJpljTio62k19C6UpuIPtW7me2sxyU2dyYg",
             "",
             "wrong"), false);
}


void test_authenticate_user() {

  auto factory = std::make_shared<basicauth_test_data_selection::factory>();

  auto txn_readonly = factory->get_default_transaction();
  auto sel = factory->make_selection(*txn_readonly);

  {
    test_request req;
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{}, "Missing Header");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{}, "Empty AUTH header");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{}, "Empty AUTH header");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbw==");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{}, "User without password");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzo=");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res,std::optional<osm_user_id_t>{}, "User and colon without password");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzpwYXNzd29yZA==");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{4711}, "Known user with correct password");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic YXJnb24yOnBhc3N3b3Jk");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{4712}, "Known user with correct password, argon2");
  }

  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic TotalCrapData==");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{}, "Crap data");
  }

  // Test with known user and incorrect password
  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzppbmNvcnJlY3Q=");
    try {
      static_cast<void>(basicauth::authenticate_user(req, *sel));
      throw std::runtime_error("Known user, incorrect password: expected http unauthorized exception");

    } catch (http::exception &e) {
      if (e.code() != 401)
        throw std::runtime_error(
            "Known user / incorrect password: Expected HTTP 401");
    }
  }

  // Test with known user and incorrect password, argon2
  {
    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic YXJnb24yOndyb25n");
    try {
      static_cast<void>(basicauth::authenticate_user(req, *sel));
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
      static_cast<void>(basicauth::authenticate_user(req, *sel));
      throw std::runtime_error("Unknown user / incorrect password: expected http unauthorized exception");

    } catch (http::exception &e) {
      if (e.code() != 401)
        throw std::runtime_error(
            "Unknown user / incorrect password: Expected HTTP 401");
    }
  }
}

void test_basic_auth_disabled_by_global_config() {

  auto factory = std::make_shared<basicauth_test_data_selection::factory>();

  auto txn_readonly = factory->get_default_transaction();
  auto sel = factory->make_selection(*txn_readonly);

  // Known user with correct password, but basic auth has been disabled in global config settings
  {
    auto test_settings = std::unique_ptr<global_settings_test_no_basic_auth>(new global_settings_test_no_basic_auth());
    global_settings::set_configuration(std::move(test_settings));

    test_request req;
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzpwYXNzd29yZA==");
    auto res = basicauth::authenticate_user(req, *sel);
    assert_equal<std::optional<osm_user_id_t> >(res, std::optional<osm_user_id_t>{}, "Known user with correct password, but basic auth disabled in global config");
  }
}


int main() {
  try {
    ANNOTATE_EXCEPTION(test_password_hash());
    ANNOTATE_EXCEPTION(test_authenticate_user());
    ANNOTATE_EXCEPTION(test_basic_auth_disabled_by_global_config());

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}



