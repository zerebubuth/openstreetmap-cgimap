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
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <filesystem>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

#include "test_request.hpp"

namespace fs = std::filesystem;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

std::map<std::string, std::string> read_headers(std::istream &in,
                                                std::string_view separator) {
  std::map<std::string, std::string> headers;

  while (true) {
    std::string line;
    std::getline(in, line);

    // allow comments in lines which begin immediately with #. this shouldn't
    // conflict with any headers, as although http headers technically can start
    // with #, i'm pretty sure we're not using any which do.
    if (line.starts_with('#')) {
      continue;
    }

    std::erase(line, '\r');

    if (!in.good()) {
      throw std::runtime_error("Test file ends before separator.");
    }
    if (line == separator) {
      break;
    }

    // Split HTTP header "Request-Method: GET" into "Request-Method" and value "GET"
    const auto pos = line.find(':');

    if (pos == std::string::npos) {
       throw std::runtime_error("Test file header doesn't match expected format.");
    }

    auto key = trim(line.substr(0, pos));
    auto value = trim(line.substr(pos + 1));
    headers.try_emplace(key, value);
  }

  return headers;
}

/**
 * take the test file and use it to set up the request headers.
 */
void setup_request_headers(test_request &req, std::istream &in) {
  auto headers = read_headers(in, "---");

  for (auto const & [k, v] : headers) {
    auto key = k;
    std::ranges::transform(key, key.begin(), [](unsigned char c) {
      return (c == '-' ? '_' : std::toupper(c));
    });

    if (key == "DATE") {
      req.set_current_time(parse_time(v));
    } else {
      req.set_header(key, v);
    }
  }

  // always set the remote addr variable
  req.set_header("REMOTE_ADDR", "127.0.0.1");
}

/**
 * check the xml attributes of two elements are the same. this is a
 * different test method because we don't care about ordering for
 * attributes, whereas the main method for XML elements does.
 */
void check_xmlattr(const pt::ptree &expected, const pt::ptree &actual) {
  std::set<std::string> exp_keys;
  std::set<std::string> act_keys;

  for (const auto &val : expected) {
    exp_keys.insert(val.first);
  }
  for (const auto &val : actual) {
    act_keys.insert(val.first);
  }

  if (exp_keys.size() > act_keys.size()) {
    for (const auto &ak : act_keys) { exp_keys.erase(ak); }
    std::ostringstream out;
    out << "Missing attributes [";
    for (const auto &ek : exp_keys) { out << ek << " "; }
    out << "]";
    throw std::runtime_error(out.str());
  }

  if (act_keys.size() > exp_keys.size()) {
    for (const auto &ek : exp_keys) { act_keys.erase(ek); }
    std::ostringstream out;
    out << "Extra attributes [";
    for (const auto &ak : act_keys) { out << ak << " "; }
    out << "]";
    throw std::runtime_error(out.str());
  }

  for (const std::string &k : exp_keys) {
    auto exp_child = expected.get_child_optional(k);
    auto act_child = actual.get_child_optional(k);

    if (exp_child) {
      if (act_child) {
        auto exp_val = exp_child->data();
        auto act_val = act_child->data();
        if ((exp_val != act_val) && (exp_val != "***")) {
          throw std::runtime_error(
            fmt::format(
              "Attribute `{}' expected value `{}', but got `{}'",
             k, exp_val,  act_val));
        }
      } else {
        throw std::runtime_error(fmt::format("Expected to find attribute `{}', but it was missing.", k));
      }
    } else if (act_child) {
      throw std::runtime_error(fmt::format("Found attribute `{}', but it was not expected to exist.", k));
    }
  }
}

/**
 * recursively check an XML tree for a match. this is a very basic way of
 * doing it, but seems effective so far. the trees are walked depth-first
 * and values are compared exactly - except for when the expected value is
 * '***', which causes it to skip that subtree entirely.
 */
void check_recursive_tree(const pt::ptree &expected, const pt::ptree &actual) {
  auto exp_itr = expected.begin();
  auto act_itr = actual.begin();

  // skip comparison of trees for this wildcard.
  if (trim(expected.data()) == "***") {
    return;
  }

  while (true) {
    if ((exp_itr == expected.end()) && (act_itr == actual.end())) {
      break;
    }
    if (exp_itr == expected.end()) {
      std::ostringstream out;
      out << "Actual result has more entries than expected: ["
          << act_itr->first;
      ++act_itr;
      while (act_itr != actual.end()) {
        out << ", " << act_itr->first;
        ++act_itr;
      }
      out << "] are extra";
      throw std::runtime_error(out.str());
    }
    if (act_itr == actual.end()) {
      std::ostringstream out;
      out << "Actual result has fewer entries than expected: ["
          << exp_itr->first;
      ++exp_itr;
      while (exp_itr != expected.end()) {
        out << ", " << exp_itr->first;
        ++exp_itr;
      }
      out << "] are absent";
      throw std::runtime_error(out.str());
    }
    if (exp_itr->first != act_itr->first) {
      throw std::runtime_error(fmt::format("Expected {}, but got {}",
                                exp_itr->first, act_itr->first));
    }
    try {
      if (exp_itr->first == "<xmlattr>") {
        check_xmlattr(exp_itr->second, act_itr->second);
      } else {
        check_recursive_tree(exp_itr->second, act_itr->second);
      }
    } catch (const std::exception &ex) {
      throw std::runtime_error(fmt::format("{}, in <{}> element",
                                ex.what(), exp_itr->first));
    }
    ++exp_itr;
    ++act_itr;
  }
}

/**
 * check that the content body of the expected, from the test case, and
 * actual, from the response, is the same.
 */
void check_content_body_xml(std::istream &expected, std::istream &actual) {
  pt::ptree exp_tree;
  pt::ptree act_tree;

  try {
    pt::read_xml(expected, exp_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        fmt::format("{}, while reading expected XML.", ex.what()));
  }

  try {
    pt::read_xml(actual, act_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        fmt::format("{}, while reading actual XML.", ex.what()));
  }

  // and check the results for equality
  check_recursive_tree(exp_tree, act_tree);
}

/**
 * recursively check a JSON tree for a match. this is a very basic way of
 * doing it, but seems effective so far. the trees are walked depth-first
 * and values are compared exactly - except for when the expected value is
 * '***', which causes it to skip that subtree entirely.
 */
void check_recursive_tree_json(const pt::ptree &expected,
                               const pt::ptree &actual) {
  auto exp_itr = expected.begin();
  auto act_itr = actual.begin();

  // skip comparison of trees for this wildcard.
  if (trim(expected.data()) == "***") {
    std::cout << "wildcard\n";
    return;
  }

  // check the actual data value
  if (expected.data() != actual.data()) {
    throw std::runtime_error(fmt::format("Expected '{}', but got '{}'",
                              expected.data(), actual.data()));
  }
  std::cout << "attr match: " << expected.data() << "\n";

  while (true) {
    if ((exp_itr == expected.end()) && (act_itr == actual.end())) {
      break;
    }
    if (exp_itr == expected.end()) {
      std::ostringstream out;
      out << "Actual result has more entries than expected: ["
          << act_itr->first;
      ++act_itr;
      while (act_itr != actual.end()) {
        out << ", " << act_itr->first;
        ++act_itr;
      }
      out << "] are extra";
      throw std::runtime_error(out.str());
    }
    if (act_itr == actual.end()) {
      std::ostringstream out;
      out << "Actual result has fewer entries than expected: ["
          << exp_itr->first;
      ++exp_itr;
      while (exp_itr != expected.end()) {
        out << ", " << exp_itr->first;
        ++exp_itr;
      }
      out << "] are absent";
      throw std::runtime_error(out.str());
    }
    if (exp_itr->first != act_itr->first) {
      throw std::runtime_error(fmt::format("Expected {}, but got {}",
                                exp_itr->first, act_itr->first));
    }
    try {
      std::cout << "recursing on item " << exp_itr->first << "\n";
      check_recursive_tree_json(exp_itr->second, act_itr->second);
    } catch (const std::exception &ex) {
      throw std::runtime_error(fmt::format("{}, in \"{}\" object",
                                ex.what(), exp_itr->first));
    }
    ++exp_itr;
    ++act_itr;
  }
  std::cout << "return\n";
}

/**
 * check that the content body of the expected, from the test case, and
 * actual, from the response, is the same.
 */
void check_content_body_json(std::istream &expected, std::istream &actual) {
  pt::ptree exp_tree;
  pt::ptree act_tree;

  try {
    pt::read_json(expected, exp_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        fmt::format("{}, while reading expected JSON.", ex.what()));
  }

  try {
    pt::read_json(actual, act_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        fmt::format("{}, while reading actual JSON.", ex.what()));
  }

  expected.seekg(0);
  std::cout << "=== expected ===\n";
  do {
    std::string line;
    std::getline(expected, line);
    std::cout << line << "\n";
  } while (expected.good());
  actual.seekg(0);
  std::cout << "=== actual ===\n";
  do {
    std::string line;
    std::getline(actual, line);
    std::cout << line << "\n";
  } while (actual.good());

  // and check the results for equality
  check_recursive_tree_json(exp_tree, act_tree);
}

void check_content_body_plain(std::istream &expected, std::istream &actual) {
  const size_t buf_size = 1024;
  char exp_buf[buf_size];
  char act_buf[buf_size];

  while (true) {
    expected.read(exp_buf, buf_size);
    actual.read(act_buf, buf_size);

    size_t exp_num = expected.gcount();
    size_t act_num = actual.gcount();

    std::string exp(exp_buf, exp_buf + exp_num);
    std::string act(act_buf, act_buf + act_num);

    if (exp_num != act_num) {
      throw std::runtime_error(
        fmt::format("Expected to read {} bytes, but read {} in actual "
                       "plain - responses are different sizes.\nexpected \"{}\", actual \"{}\"", exp_num, act_num, exp, act));
    }

    if (!std::equal(exp_buf, exp_buf + exp_num, act_buf)) {
      throw std::runtime_error(
        fmt::format("Returned content differs: expected \"{}\", actual "
                       "\"{}\" - responses are different.", exp, act));
    }

    if (expected.eof() && actual.eof()) {
      break;
    }
  }
}

using dict = std::map<std::string, std::string>;

std::ostream &operator<<(std::ostream &out, const dict &d) {
  for (const auto & [key, value] : d) {
    out << key << ": " << value << "\n";
  }
  return out;
}

void check_headers(const dict &expected_headers,
                   const dict &actual_headers) {
  for (const auto & [key, value] : expected_headers) {
    if (key.starts_with('!')) {
      if (actual_headers.contains(key.substr(1))) {
        throw std::runtime_error(
          fmt::format(
            "Expected not to find header `{}', but it is present.",
            key.substr(1)));
      }
    } else {
      if (!actual_headers.contains(key)) {
        throw std::runtime_error(
          fmt::format("Expected header `{}: {}', but didn't find it in "
                         "actual response.",
           key, value));
      }
      if (!value.empty()) {
        const auto & actual_value = actual_headers.at(key);
        if (value != actual_value) {
          throw std::runtime_error(
            fmt::format(
              "Header key `{}'; expected `{}' but got `{}'.",
             key, value, actual_value));
        }
      }
    }
  }
}

/**
 * Check the response from cgimap against the expected test result
 * from the test file.
 */
void check_response(std::istream &expected, std::istream &actual) {
  // check that, for some headers that we get, they are the same
  // as we expect.
  const auto expected_headers = read_headers(expected, "---");
  const auto actual_headers = read_headers(actual, "");

  try {
    check_headers(expected_headers, actual_headers);

  } catch (const std::runtime_error &e) {
    std::ostringstream out;
    out << "While comparing expected headers:\n"
        << expected_headers << "\n"
        << "with actual headers:\n"
        << actual_headers << "\n"
        << "ERROR: " << e.what();
    throw std::runtime_error(out.str());
  }

  // now check the body, if there is one. we judge this by whether we expect a
  // Content-Type header.
  if (expected_headers.contains("Content-Type")) {
    const auto content_type = expected_headers.at("Content-Type");
    if (content_type.starts_with("text/xml") ||
	      content_type.starts_with("application/xml") ||
        content_type.starts_with("text/html")) {
      check_content_body_xml(expected, actual);

    } else if (content_type.starts_with("application/json")) {
      check_content_body_json(expected, actual);

    } else if (content_type.starts_with("text/plain")) {
      check_content_body_plain(expected, actual);

    } else {
      throw std::runtime_error(
          fmt::format("Cannot yet handle tests with Content-Type: {}.",
           content_type));
    }
  }
}

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

osm_user_role_t parse_role(std::string_view str) {
  using enum osm_user_role_t;
  if (str == "administrator") {
    return administrator;
  } else if (str == "moderator") {
    return moderator;
  } else if (str == "importer") {
    return importer;
  }
  throw std::runtime_error("Unable to parse role in config file.");
}

user_roles_t parse_user_roles(const pt::ptree &config) {

  user_roles_t user_roles;
  auto users = config.get_child_optional("users");
  if (users)
  {
    for (const auto &entry : *users)
    {
      auto id = boost::lexical_cast< osm_user_id_t >(entry.first);
      auto roles = entry.second.get_child_optional("roles");

      std::set< osm_user_role_t > r;
      if (roles)
      {
        for (const auto &role : *roles)
        {
          r.insert(parse_role(role.second.get_value< std::string >()));
        }
      }

      user_roles.emplace(id, std::move(r));
    }
  }
  return user_roles;
}

user_roles_t get_user_roles(const fs::path &roles_file) {

  if (!fs::is_regular_file(roles_file))
    return {};

  try
  {
    pt::ptree config;
    pt::read_json(roles_file.string(), config);
    return parse_user_roles(config);
  }
  catch (const std::exception &ex)
  {
    throw std::runtime_error(
        fmt::format("{}, while reading expected JSON.", ex.what()));
  }
}


oauth2_tokens parse_oauth2_tokens(const pt::ptree &config) {

  oauth2_tokens oauth2_tokens;
  auto tokens = config.get_child_optional("tokens");
  if (tokens)
  {
    for (const auto &[token, attrs] : *tokens)
    {
      oauth2_token_detail_t detail{ .expired = attrs.get<bool>("expired", true),
                                    .revoked = attrs.get<bool>("revoked", true),
                                    .api_write = attrs.get<bool>("api_write", false),
                                    .user_id = attrs.get<osm_user_id_t>("user_id", {}) };
      oauth2_tokens[token] = detail;
    }
  }
  return oauth2_tokens;
}

oauth2_tokens get_oauth2_tokens(const fs::path &oauth2_file) {

  if (!fs::is_regular_file(oauth2_file))
    return {};

  try
  {
    pt::ptree config;
    pt::read_json(oauth2_file.string(), config);
    return parse_oauth2_tokens(config);
  }
  catch (const std::exception &ex)
  {
    throw std::runtime_error(fmt::format("{}, while reading expected JSON.", ex.what()));
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
    const fs::directory_iterator end;
    for (fs::directory_iterator itr(test_directory); itr != end; ++itr) {
      fs::path filename = itr->path();
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
