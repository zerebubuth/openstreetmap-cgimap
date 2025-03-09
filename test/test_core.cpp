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

#include <boost/program_options.hpp>
#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

#include "test_core_helper.hpp"
#include "test_request.hpp"

namespace fs = std::filesystem;

/**
 * Main test body:
 *  - reads the test case,
 *  - constructs a matching mock request,
 *  - executes it through the standard process_request() chain,
 *  - compares the result to what's expected in the test case.
 */
void run_test(fs::path test_case, rate_limiter &limiter,
              const std::string &generator, const routes &route,
              data_selection::factory& factory) {
  try {
    test_request req;

    // set up request headers from test case
    std::ifstream in(test_case, std::ios::binary);
    setup_request_headers(req, in);

    // execute the request
    process_request(req, limiter, generator, route, factory, nullptr);

    // compare the result to what we're expecting
    try {
      check_response(in, req.buffer());

    } catch (const std::exception &e) {
      if (getenv("VERBOSE") != nullptr) {
        std::cout << "ERROR: " << e.what() << "\n\n"
                  << "Response was:\n----------------------\n"
                  << req.buffer().str() << "\n";
      }
      throw;
    }

    // output test case name if verbose output is requested
    if (getenv("VERBOSE") != nullptr) {
      std::cout << "PASS: " << test_case << "\n";
    }

  } catch (const std::exception &ex) {
    throw std::runtime_error(
        fmt::format("{}, in {} test.", ex.what(), test_case.string()));
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <test-directory>." << std::endl;
    return 99;
  }

  fs::path test_directory = argv[1];
  fs::path data_file = test_directory / "data.osm";
  fs::path oauth2_file = test_directory / "oauth2.json";
  fs::path roles_file = test_directory / "roles.json";
  std::vector<fs::path> test_cases;

  user_roles_t user_roles;
  oauth2_tokens oauth2_tokens;

  try {
    if (!fs::is_directory(test_directory)) {
      std::cerr << "Test directory " << test_directory
                << " should be a directory, but isn't.";
      return 99;
    }
    if (!fs::is_regular_file(data_file)) {
      std::cerr << "Test directory should contain data file at " << data_file
                << ", but does not.";
      return 99;
    }
    for (const auto& entry : fs::directory_iterator(test_directory)) {
      fs::path filename = entry.path();
      std::string ext = filename.extension();
      if (ext == ".case") {
        test_cases.push_back(filename);
      }
    }

    user_roles = get_user_roles(roles_file);
    oauth2_tokens = get_oauth2_tokens(oauth2_file);

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 99;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 99;
  }

  try {
    po::variables_map vm;
    vm.try_emplace("file", po::variable_value(data_file.native(), false));

    auto data_backend = make_staticxml_backend(user_roles, oauth2_tokens);
    auto factory = data_backend->create(vm);
    null_rate_limiter limiter;
    routes route;

    for (fs::path test_case : test_cases) {
      std::string generator = fmt::format(PACKAGE_STRING " (test {})", test_case.string());
      run_test(test_case, limiter, generator, route, *factory);
    }

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}
