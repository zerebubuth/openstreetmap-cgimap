/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <fmt/core.h>
#include <boost/program_options.hpp>

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


/***********************************************************************************
 * TODO: Rename test case to test_apidb_backend_role.cpp
 ***********************************************************************************/

using roles_t = std::set<osm_user_role_t>;

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

void test_get_roles_for_user(test_database &tdb) {
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

  auto sel = tdb.get_data_selection();

  // user 3 has no roles -> should return empty set
  assert_equal<roles_t>(roles_t(), sel->get_roles_for_user(3),
    "roles for normal user");

  // user 2 is a moderator
  assert_equal<roles_t>(
    roles_t({osm_user_role_t::moderator}),
    sel->get_roles_for_user(2),
    "roles for moderator user");

  // user 1 is an administrator and a moderator
  assert_equal<roles_t>(
    roles_t({osm_user_role_t::moderator, osm_user_role_t::administrator}),
    sel->get_roles_for_user(1),
    "roles for admin+moderator user");
}


} // anonymous namespace

int main(int argc, char** argv) {
  std::filesystem::path test_db_sql;
  boost::program_options::options_description desc("Allowed options");
  desc.add_options()
      ("help", "print help message")
      ("db-schema", po::value<std::filesystem::path>(&test_db_sql)->default_value("test/structure.sql"), "test database schema")
  ;

  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  try {
    test_database tdb;
    tdb.setup(test_db_sql);

    tdb.run(std::function<void(test_database&)>(
        &test_get_roles_for_user));

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
