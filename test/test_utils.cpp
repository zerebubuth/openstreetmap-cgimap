/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

/* -*- coding: utf-8 -*- */
#include "cgimap/util.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE("util_escape", "[util]") {
  CHECK(escape("") == "\"\"");
  CHECK(escape("abcd1234") == "\"abcd1234\"");
  CHECK(escape("ab\\\\c\\\"d1'234") == "\"ab\\\\\\\\c\\\\\\\"d1'234\"");
}

TEST_CASE("util_join_comma_separated", "[util]") {
  CHECK(to_string<std::vector<int>>({}) == "");
  CHECK(to_string<std::vector<int>>({1}) == "1");
  CHECK(to_string<std::vector<int>>({1, 2, 3, 4}) == "1,2,3,4");
}

TEST_CASE("util_parse_ruby_number", "[util]") {
  SECTION("Valid number") {
    CHECK(parse_ruby_number<int>("1") == 1);
    CHECK(parse_ruby_number<int>("235678") == 235678);
    CHECK(parse_ruby_number<int>("-1") == -1);
    CHECK(parse_ruby_number<uint32_t>("123") == 123);
  }

  SECTION("Number prefix") {
    CHECK(parse_ruby_number<uint32_t>("123abc") == 123);
    CHECK(parse_ruby_number<uint32_t>("1 2") == 1);
  }

  SECTION("Invalid numbers") {
    CHECK(parse_ruby_number<uint32_t>("-1") == 0);
    CHECK(parse_ruby_number<int>("9999999999999999999999999") == 0);
    CHECK(parse_ruby_number<uint32_t>("abc123") == 0);
    CHECK(parse_ruby_number<uint32_t>("0x123") == 0);
  }
}
