/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include "cgimap/options.hpp"

#include <string>
#include <stdexcept>

#include <boost/program_options.hpp>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

namespace po = boost::program_options;

void check_options(const po::variables_map& options)
{
  global_settings::set_configuration(std::make_unique<global_settings_via_options>(options));
}

TEST_CASE("No command line options", "[options]") {
    po::variables_map vm;
    REQUIRE_NOTHROW(check_options(vm));
}

TEST_CASE("Invalid max-payload", "[options]") {
    po::variables_map vm;
    vm.emplace("max-payload", po::variable_value((long) -1, false));
    REQUIRE_THROWS_AS(check_options(vm), std::invalid_argument);
}

TEST_CASE("Set all supported options" "[options]") {
  po::variables_map vm;
  vm.emplace("max-payload", po::variable_value((long) 40000, false));
  vm.emplace("map-nodes", po::variable_value((int) 1000, false));
  vm.emplace("map-area",  po::variable_value((double) 0.1, false));
  vm.emplace("changeset-timeout-open", po::variable_value(std::string("10 minutes"), false));
  vm.emplace("changeset-timeout-idle", po::variable_value(std::string("1 hour"), false));
  vm.emplace("max-changeset-elements", po::variable_value((int) 1000, false));
  vm.emplace("max-way-nodes", po::variable_value((int) 100, false));
  vm.emplace("scale", po::variable_value((long) 100, false));
  vm.emplace("max-way-nodes", po::variable_value((int) 200, false));
  vm.emplace("max-relation-members", po::variable_value((int) 50, false));
  vm.emplace("max-element-tags", po::variable_value((int) 10, false));
  vm.emplace("ratelimit", po::variable_value((long) 1000000, false));
  vm.emplace("moderator-ratelimit", po::variable_value((long) 10000000, false));
  vm.emplace("maxdebt", po::variable_value((long) 500, false));
  vm.emplace("moderator-maxdebt", po::variable_value((long) 1000, false));
  vm.emplace("ratelimit-upload", po::variable_value(true, false));
  REQUIRE_NOTHROW(check_options(vm));

  REQUIRE( global_settings::get_payload_max_size() == 40000 );
  REQUIRE( global_settings::get_map_max_nodes() == 1000 );
  REQUIRE( global_settings::get_map_area_max() == 0.1 );
  REQUIRE( global_settings::get_changeset_timeout_open_max() == "10 minutes" );
  REQUIRE( global_settings::get_changeset_timeout_idle() == "1 hour" );
  REQUIRE( global_settings::get_changeset_max_elements() == 1000 );
  REQUIRE( global_settings::get_way_max_nodes() == 100 );
  REQUIRE( global_settings::get_scale() == 100 );
  REQUIRE( global_settings::get_relation_max_members() == 50 );
  REQUIRE( global_settings::get_element_max_tags() == 10 );
  REQUIRE( global_settings::get_ratelimiter_ratelimit(false) == 1000000 );
  REQUIRE( global_settings::get_ratelimiter_maxdebt(false) == 500l * 1024 * 1024 );
  REQUIRE( global_settings::get_ratelimiter_ratelimit(true) == 10000000 );
  REQUIRE( global_settings::get_ratelimiter_maxdebt(true) == 1000l * 1024 * 1024 );
  REQUIRE( global_settings::get_ratelimiter_upload() == true );
}
