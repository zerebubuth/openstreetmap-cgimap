/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/handler_utils.hpp"
#include "test_request.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <string_view>
#include <string>

std::vector<api06::id_version> parse_query_str(const std::string& query_str) {
  test_request req;
  req.set_header("REQUEST_METHOD", "GET");
  req.set_header("QUERY_STRING", query_str);
  req.set_header("PATH_INFO", "/api/0.6/nodes");
  return api06::parse_id_list_params(req, "nodes");
}

TEST_CASE("Id list returns no duplicates", "[idlist]") {
  // the container returned from parse_id_list_params should not contain any duplicates.
  std::string query_str = "nodes=1,1,1,1";
  auto ids = parse_query_str(query_str);
  CHECK(ids.size() == 1);
}

TEST_CASE("Id list parse negative nodes", "[idlist]") {

  std::string query_str =
    "nodes=-1875196430,1970729486,-715595887,153329585,276538320,276538320,"
    "276538320,276538320,557671215,268800768,268800768,272134694,416571249,"
    "4118507737,639141976,-120408340,4118507737,4118507737,-176459559,"
    "-176459559,-176459559,416571249,-176459559,-176459559,-176459559,"
    "557671215";

  auto ids = parse_query_str(query_str);

  // maximum ID that postgres can handle is 2^63-1, so that should never
  // be returned by the parsing function.
  const osm_nwr_id_t max_id = std::numeric_limits<int64_t>::max();
  for (api06::id_version idv : ids) {
    CHECK (idv.id < max_id);
  }
}

TEST_CASE("Missing id list", "[idlist]") {
  std::string query_str = "nodes=";
  std::vector<api06::id_version> ids;
  REQUIRE_NOTHROW(ids = parse_query_str(query_str));
  CHECK(ids.empty() == true);
}

TEST_CASE("Id list with garbage", "[idlist]") {
  std::string query_str = "nodes=\xf5";
  std::vector<api06::id_version> ids;
  REQUIRE_NOTHROW(ids = parse_query_str(query_str));
  CHECK(ids.size() == 1);
  CHECK(ids[0] == api06::id_version());
}

TEST_CASE("Id list with history", "[idlist]") {
  std::string query_str = "nodes=1,1v1";
  auto ids = parse_query_str(query_str);

  CHECK(ids.size() == 2);

  // NOTE: ID list is uniqued and sorted, which puts the "latest" version at the end.
  CHECK(ids[0] == api06::id_version(1, 1));
  CHECK(ids[1] == api06::id_version(1));
}
