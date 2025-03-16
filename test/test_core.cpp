/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/process_request.hpp"
#include "cgimap/time.hpp"
#include "cgimap/util.hpp"
#include "staticxml.hpp"

#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

#include "test_core_helper.hpp"
#include "test_request.hpp"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

namespace fs = std::filesystem;

fs::path test_directory{};

std::vector<fs::path> get_test_cases() {
  std::vector<fs::path> test_cases;

  for (const auto& entry : fs::directory_iterator(test_directory)) {
    fs::path filename = entry.path();
    if (filename.extension() == ".case") {
      test_cases.push_back(filename);
    }
  }
  return test_cases;
}

namespace global {
  user_roles_t test_user_roles;
  oauth2_tokens test_oauth2_tokens;
  null_rate_limiter limiter;
  routes route;
  po::variables_map vm;

  std::vector<fs::path> test_cases;

  fs::path data_file;
  fs::path oauth2_file;
  fs::path roles_file;

  std::unique_ptr<backend> data_backend;
  std::unique_ptr<data_selection::factory> factory;
}

TEST_CASE("Execute core test cases using external test data") {

  SECTION("Initialize test data") {

    using namespace global;

    if (test_directory.empty()) {
      FAIL("No test directory specified. Missing --test-directory command line option.");
    }

    data_file = test_directory / "data.osm";
    oauth2_file = test_directory / "oauth2.json";
    roles_file = test_directory / "roles.json";

    if (!fs::is_directory(test_directory)) {
      FAIL("Test directory does not exist.");
    }

    if (!fs::is_regular_file(data_file)) {
      FAIL("data.osm file does not exist in given test directory.");
    }

    REQUIRE_NOTHROW(test_user_roles = get_user_roles(roles_file));
    REQUIRE_NOTHROW(test_oauth2_tokens = get_oauth2_tokens(oauth2_file));

    test_cases = get_test_cases();
    if(test_cases.empty()) {
      FAIL("No test cases found in the test directory.");
    }

    // Prepare the backend with the test data
    vm.try_emplace("file", po::variable_value(data_file.native(), false));

    data_backend = make_staticxml_backend(test_user_roles, test_oauth2_tokens);
    factory = data_backend->create(vm);
  }

  SECTION("Execute test cases") {

    using namespace global;

    // Execute actual test cases
    for (fs::path test_case : test_cases) {
      SECTION("Execute single testcase " + test_case.filename().string()) {
        std::string generator = fmt::format(PACKAGE_STRING " (test {})", test_case.string());
        test_request req;

        // set up request headers from test case
        std::ifstream in(test_case, std::ios::binary);
        setup_request_headers(req, in);

        // execute the request
        REQUIRE_NOTHROW(process_request(req, limiter, generator, route, *factory, nullptr));

        CAPTURE(req.body().str());
        REQUIRE_NOTHROW(check_response(in, req.buffer()));
      }
    }
  }
}


int main(int argc, char *argv[]) {
  Catch::Session session;

  using namespace Catch::clara;
  auto cli =
      session.cli()
      | Opt(test_directory,
            "test-directory")
            ["--test-directory"]
      ("test case directory");

  session.cli(cli);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0)
    return returnCode;

  return session.run();
}