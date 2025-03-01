/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <iostream>
#include <stdexcept>
#include <fmt/core.h>

#include <sys/time.h>
#include <cstdio>
#include <memory>

#include "cgimap/time.hpp"
#include "cgimap/options.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/process_request.hpp"

#include "test_formatter.hpp"
#include "test_database.hpp"
#include "test_request.hpp"
#include "test_empty_selection.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

using roles_t = std::set<osm_user_role_t>;


class DatabaseTestsFixture
{
protected:
  DatabaseTestsFixture() = default;
  static test_database tdb;
};

test_database DatabaseTestsFixture::tdb{};

struct CGImapListener : Catch::TestEventListenerBase, DatabaseTestsFixture {

    using TestEventListenerBase::TestEventListenerBase; // inherit constructor

    void testRunStarting( Catch::TestRunInfo const& testRunInfo ) override {
      // load database schema when starting up tests
      tdb.setup();
    }

    void testCaseStarting( Catch::TestCaseInfo const& testInfo ) override {
      tdb.testcase_starting();
    }

    void testCaseEnded( Catch::TestCaseStats const& testCaseStats ) override {
      tdb.testcase_ended();
    }
};

CATCH_REGISTER_LISTENER( CGImapListener )

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
    } else if (r == osm_user_role_t::importer) {
      out << "importer";
    }
  }
  out << "}";
  return out;
}

template <> struct fmt::formatter<roles_t> {
  template <typename FormatContext>
  auto format(const roles_t& r, FormatContext& ctx) -> decltype(ctx.out()) {
    // ctx.out() is an output iterator to write to.
    std::ostringstream ostr;
    ostr << r;
    return format_to(ctx.out(), "{}", ostr.str());
  }
  constexpr auto parse(const format_parse_context& ctx) const { return ctx.begin(); }
};




template<typename T>
std::ostream& operator<<(std::ostream& os, std::optional<T> const& opt)
{
  return opt ? os << opt.value() : os;
}


TEST_CASE_METHOD(DatabaseTestsFixture, "test_get_roles_for_user", "[roles][db]" ) {

  auto sel = tdb.get_data_selection();

  SECTION("Initialize test data") {


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

  }

  // user 3 has no roles -> should return empty set
  REQUIRE(roles_t() == sel->get_roles_for_user(3));

  // user 2 is a moderator
  REQUIRE(roles_t({osm_user_role_t::moderator}) == sel->get_roles_for_user(2));

  // user 1 is an administrator and a moderator
  REQUIRE(roles_t({osm_user_role_t::moderator, osm_user_role_t::administrator}) == sel->get_roles_for_user(1));
}


