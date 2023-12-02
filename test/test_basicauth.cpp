/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "cgimap/basicauth.hpp"
#include "cgimap/options.hpp"
#include "test_request.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

namespace {

class basicauth_test_data_selection : public data_selection {
public:

  ~basicauth_test_data_selection() override = default;

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
  bool is_user_active(const osm_user_id_t) override { return false; }
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

  struct factory : public data_selection::factory {
    ~factory() override = default;
    std::unique_ptr<data_selection> make_selection(Transaction_Owner_Base&) const override {
      return std::make_unique<basicauth_test_data_selection>();
    }
    std::unique_ptr<Transaction_Owner_Base> get_default_transaction() override {
      return std::make_unique<Transaction_Owner_Void>();
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

TEST_CASE("test_md5_without_salt", "[basicauth]") {
  CHECK(PasswordHash::check("5f4dcc3b5aa765d61d8327deb882cf99", "","password") == true);
  CHECK(PasswordHash::check("5f4dcc3b5aa765d61d8327deb882cf99", "", "wrong") == false);
}

TEST_CASE("test_md5_with_salt", "[basicauth]") {
  CHECK(PasswordHash::check("67a1e09bb1f83f5007dc119c14d663aa", "salt", "password") == true);
  CHECK(PasswordHash::check("67a1e09bb1f83f5007dc119c14d663aa", "salt", "wrong") == false);
  CHECK(PasswordHash::check("67a1e09bb1f83f5007dc119c14d663aa", "wrong","password") == false);
}

TEST_CASE("test_pbkdf2_1000_32_sha512", "[basicauth]") {
  CHECK(PasswordHash::check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "password") == true);

  CHECK(PasswordHash::check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gpbRybtoNzm/CNKe4cf7bPKwdUNrk=",
             "wrong") == false);

  CHECK(PasswordHash::check(
             "ApT/28+FsTBLa/J8paWfgU84SoRiTfeY8HjKWhgHy08=",
             "sha512!1000!HR4z+hAvKV2ra1gwrongtoNzm/CNKe4cf7bPKwdUNrk=",
             "password") == false);
}


TEST_CASE("test_pbkdf2_10000_32_sha512", "[basicauth]") {

  CHECK(PasswordHash::check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password") == true);

  CHECK(PasswordHash::check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtM7eD8huvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "wrong") == false);

  CHECK(PasswordHash::check(
             "3wYbPiOxk/tU0eeIDjUhdvi8aDP3AbFtwYKKxF1IhGg=",
             "sha512!10000!OUQLgtMwronguvanFT5/WtWaCwdOdrir8QOtFwxhO0A=",
             "password") == false);
}

TEST_CASE("test argon2", "[basicauth]") {

  CHECK(PasswordHash::check(
             "$argon2id$v=19$m=65536,t=1,p=1$KXGHWfWMf5H5kY4uU3ua8A$YroVvX6cpJpljTio62k19C6UpuIPtW7me2sxyU2dyYg",
             "",
             "password") == true);

  CHECK(PasswordHash::check(
             "$argon2id$v=19$m=65536,t=1,p=1$KXGHWfWMf5H5kY4uU3ua8A$YroVvX6cpJpljTio62k19C6UpuIPtW7me2sxyU2dyYg",
             "",
             "wrong") == false);
}


TEST_CASE("test_authenticate_user", "[basicauth]") {

  auto factory = std::make_shared<basicauth_test_data_selection::factory>();
  auto txn_readonly = factory->get_default_transaction();
  auto sel = factory->make_selection(*txn_readonly);
  test_request req;

  SECTION("Missing Header") {
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Empty AUTH header"){
    req.set_header("HTTP_AUTHORIZATION","");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Empty AUTH header"){
    req.set_header("HTTP_AUTHORIZATION","Basic ");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("User without password"){
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbw==");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("User and colon without password"){
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzo=");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Known user with correct password"){
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzpwYXNzd29yZA==");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{4711});
  }

  SECTION("Known user with correct password, argon2") {
    req.set_header("HTTP_AUTHORIZATION","Basic YXJnb24yOnBhc3N3b3Jk");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{4712});
  }

  SECTION("Crap data") {
    req.set_header("HTTP_AUTHORIZATION","Basic TotalCrapData==");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{});
  }

  SECTION("Test with known user and incorrect password") {
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzppbmNvcnJlY3Q=");
    REQUIRE_THROWS_AS(static_cast<void>(basicauth::authenticate_user(req, *sel)), http::unauthorized);
  }

  SECTION("Test with known user and incorrect password, argon2") {
    req.set_header("HTTP_AUTHORIZATION","Basic YXJnb24yOndyb25n");
    REQUIRE_THROWS_AS(static_cast<void>(basicauth::authenticate_user(req, *sel)), http::unauthorized);
  }

  SECTION("Test with unknown user and incorrect password") {
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzI6aW5jb3JyZWN0");
    REQUIRE_THROWS_AS(static_cast<void>(basicauth::authenticate_user(req, *sel)), http::unauthorized);
  }

  SECTION("Known user with correct password; basicauth disabled in config") {
    auto test_settings = std::unique_ptr<global_settings_test_no_basic_auth>(new global_settings_test_no_basic_auth());
    global_settings::set_configuration(std::move(test_settings));
    req.set_header("HTTP_AUTHORIZATION","Basic ZGVtbzpwYXNzd29yZA==");
    auto res = basicauth::authenticate_user(req, *sel);
    CHECK(res == std::optional<osm_user_id_t>{});
  }
}

