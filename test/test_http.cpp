/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

/* -*- coding: utf-8 -*- */
#include "cgimap/http.hpp"
#include "cgimap/choose_formatter.hpp"


#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

struct test_responder : responder {

  test_responder(mime::type t) : responder(t) {}

  std::vector<mime::type> types_available() const override {
    return {mime::type::application_json, mime::type::application_xml};
  }

  void write(output_formatter& fmt,
    const std::string &generator,
    const std::chrono::system_clock::time_point &now) override {}
};

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
  CHECK(http::choose_encoding("deflate, gzip;q=1.0, *;q=0.5")->name() == "deflate");
  CHECK(http::choose_encoding("gzip;q=1.0, identity;q=0.8, *;q=0.1")->name() == "gzip");
  CHECK(http::choose_encoding("identity;q=0.8, gzip;q=1.0, *;q=0.1")->name() == "gzip");
  CHECK(http::choose_encoding("gzip")->name() == "gzip");
  CHECK(http::choose_encoding("identity")->name() == "identity");
  CHECK(http::choose_encoding("*")->name() == "br");
  CHECK(http::choose_encoding("deflate")->name() == "deflate");
#if HAVE_BROTLI
  CHECK(http::choose_encoding("gzip, deflate, br")->name() == "br");
  CHECK(http::choose_encoding("zstd;q=1.0, deflate;q=0.8, br;q=0.9")->name() == "br");
  CHECK(http::choose_encoding("zstd;q=1.0, unknown;q=0.8, br;q=0.9")->name() == "br");
  CHECK(http::choose_encoding("gzip, deflate, br")->name() == "br");
#endif
}

TEST_CASE("http_check_accept_header_parsing", "[http]") {
  SECTION("test: RFC 2616 sample header") {
    AcceptHeader header("text/*;q=0.3, text/html;q=0.7, text/html;level=1, text/html;level=2;q=0.4, */*;q=0.5");
    CHECK(header.is_acceptable(mime::type::any_type) == true);
    CHECK(header.is_acceptable(mime::type::application_json) == true);
    CHECK(header.is_acceptable(mime::type::application_xml) == true);
    CHECK(header.is_acceptable(mime::type::text_plain) == true);
  }

  SECTION("test: wildcard header") {
    AcceptHeader header{"*/*"};
    CHECK(header.is_acceptable(mime::type::any_type) == true);
    CHECK(header.is_acceptable(mime::type::application_json) == true);
    CHECK(header.is_acceptable(mime::type::application_xml) == true);
    CHECK(header.is_acceptable(mime::type::text_plain) == true);
  }

  SECTION("test: bug-compatible wildcard header ") {
    AcceptHeader header{"*"};
    CHECK(header.is_acceptable(mime::type::any_type) == true);
    CHECK(header.is_acceptable(mime::type::application_json) == true);
    CHECK(header.is_acceptable(mime::type::application_xml) == true);
    CHECK(header.is_acceptable(mime::type::text_plain) == true);
  }

  SECTION("test: not supported mime types") {
    CHECK_NOTHROW(AcceptHeader{"audio/*; q=0.2, audio/basic"});
    CHECK_NOTHROW(AcceptHeader{"text/html"});
  }

  SECTION("test: invalid accept header format") {
    CHECK_THROWS_AS(AcceptHeader{""}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"/"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"*/"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"foo/"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"/*"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"/foo"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"*/foo"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"text"}, http::bad_request);
  }

  SECTION("test: accept header params") {
    CHECK_NOTHROW(AcceptHeader{"application/xml;q=0.5"});
    CHECK_NOTHROW(AcceptHeader{"application/xml;baz=abc;bat=123"});
    CHECK_NOTHROW(AcceptHeader{"application/xml;baz=abc;bat=123, application/json; param1=1; param2=2"});
    CHECK_NOTHROW(AcceptHeader{"foo/bar;q=0.5; accept-extension-param1=abcd123; exptaram=%653"});
    CHECK_NOTHROW(AcceptHeader{"text/html, application/xhtml+xml, application/xml;q=0.9, image/webp, */*;q=0.8"});
    CHECK_NOTHROW(AcceptHeader{"text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"});
    CHECK_NOTHROW(AcceptHeader{"text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/png,image/svg+xml,*/*;q=0.8"});
  }

  SECTION("test: invalid accept header q value") {
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=foobar"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q="}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=123456"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=-123456"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=1.1"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=-0.1"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=NAN"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=-NAN"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=INF"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=INFINITY"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=-INF"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=-INFINITY"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=0x1"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=0x0"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=0b1"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=0.5abc"}, http::bad_request);
    CHECK_THROWS_AS(AcceptHeader{"application/xml;q=abc0.5"}, http::bad_request);
  }

  SECTION("test: application/json") {
    AcceptHeader header("application/json, text/javascript");

    test_responder tr1{mime::type::application_json};
    CHECK(choose_best_mime_type(header, tr1, "/demo") == mime::type::application_json);

    test_responder tr2{mime::type::unspecified_type};
    CHECK(choose_best_mime_type(header, tr2, "/demo") == mime::type::application_json);
  }

}


