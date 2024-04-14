/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

/* -*- coding: utf-8 -*- */
#include "cgimap/http.hpp"
#include <stdexcept>
#include <iostream>
#include <sstream>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>


TEST_CASE("http_check_urlencoding", "[http]") {
  // RFC 3986 section 2.5
  CHECK(http::urlencode("ア") == "%E3%82%A2");
  CHECK(http::urlencode("À") == "%C3%80");

  // RFC 3986 - unreserved characters not encoded
  CHECK(http::urlencode("abcdefghijklmnopqrstuvwxyz") == "abcdefghijklmnopqrstuvwxyz");
  CHECK(http::urlencode("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  CHECK(http::urlencode("0123456789") == "0123456789");
  CHECK(http::urlencode("-._~") == "-._~");

  // RFC 3986 - % must be encoded
  CHECK(http::urlencode("%") == "%25");
}

TEST_CASE("http_check_urldecoding", "[http]") {
  CHECK(http::urldecode("%E3%82%A2") == "ア");
  CHECK(http::urldecode("%C3%80") == "À");

  // RFC 3986 - uppercase A-F are equivalent to lowercase a-f
  CHECK(http::urldecode("%e3%82%a2") == "ア");
  CHECK(http::urldecode("%c3%80") == "À");
}

TEST_CASE("http_check_parse_params", "[http]") {
  using params_t = std::vector<std::pair<std::string, std::string> >;
  params_t params = http::parse_params("a2=r%20b&a3=2%20q&a3=a&b5=%3D%253D&c%40=&c2=");

  CHECK(params.size() == 6);
  CHECK(params[0].first == "a2");   CHECK(params[0].second == "r%20b");
  CHECK(params[1].first == "a3");   CHECK(params[1].second == "2%20q");
  CHECK(params[2].first == "a3");   CHECK(params[2].second == "a");
  CHECK(params[3].first == "b5");   CHECK(params[3].second == "%3D%253D");
  CHECK(params[4].first == "c%40"); CHECK(params[4].second == "");
  CHECK(params[5].first == "c2");   CHECK(params[5].second == "");
}

TEST_CASE("http_check_list_methods", "[http]") {
  CHECK(http::list_methods(http::method::GET) == "GET");
  CHECK(http::list_methods(http::method::POST) == "POST");
  CHECK(http::list_methods(http::method::HEAD) == "HEAD");
  CHECK(http::list_methods(http::method::OPTIONS) == "OPTIONS");
  CHECK(http::list_methods(http::method::GET | http::method::OPTIONS) == "GET, OPTIONS");
}

TEST_CASE("http_check_parse_methods", "[http]") {
  CHECK(http::parse_method("GET") == http::method::GET);
  CHECK(http::parse_method("POST") == http::method::POST);
  CHECK(http::parse_method("HEAD") == http::method::HEAD);
  CHECK(http::parse_method("OPTIONS") == http::method::OPTIONS);
  CHECK(http::parse_method("") == std::optional<http::method>{});
}

TEST_CASE("http_check_choose_encoding", "[http]") {
  CHECK(http::choose_encoding("deflate, gzip;q=1.0, *;q=0.5")->name() == "gzip");
  CHECK(http::choose_encoding("gzip;q=1.0, identity;q=0.8, *;q=0.1")->name() == "gzip");
  CHECK(http::choose_encoding("identity;q=0.8, gzip;q=1.0, *;q=0.1")->name() == "gzip");
  CHECK(http::choose_encoding("gzip")->name() == "gzip");
  CHECK(http::choose_encoding("identity")->name() == "identity");
  CHECK(http::choose_encoding("*")->name() == "identity");
  CHECK(http::choose_encoding("deflate")->name() == "identity");
#if HAVE_BROTLI
  CHECK(http::choose_encoding("gzip, deflate, br")->name() == "br");
#endif
}
