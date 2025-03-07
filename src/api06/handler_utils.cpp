/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/handler_utils.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/util.hpp"

#include <algorithm>
#include <vector>

#include <fmt/core.h>

namespace api06 {


// Helper function to parse a single id_version string like "123" or "123v2"
id_version parse_single_id(std::string_view str) {
  id_version result{};

  // Split string at 'v'
  std::vector<std::string_view> id_version = split(str, 'v');

  switch (id_version.size()) {

  case 1:
    // Just ID without version
    result.id = parse_ruby_number<osm_nwr_id_t>(id_version[0]);
    result.version = std::nullopt;
    break;
  case 2:
    // ID with version
    result.id = parse_ruby_number<osm_nwr_id_t>(id_version[0]);
    result.version = parse_ruby_number<uint32_t>(id_version[1]);
    break;
  }

  return result;
}

std::vector<id_version> parse_id_list_params(const request &req,
                                             std::string_view param_name) {

  std::string decoded = http::urldecode(get_query_string(req));
  const auto params = http::parse_params(decoded);

  auto itr = std::ranges::find_if(params, [&param_name](auto &x) {
    return x.first == param_name;
  });

  if (itr == params.end())
    return {};

  std::string_view str = itr->second;

  if (str.empty())
    return {};

  std::vector<id_version> myids;

  // Split the input string by commas
  std::vector<std::string_view> id_strings = split(str, ',');

  // Parse each id_version string
  for (const auto &id_str : id_strings) {
    if (!id_str.empty()) {
      myids.push_back(parse_single_id(id_str));
    }
  }

  // ensure list of IDs is unique
  std::ranges::sort(myids);
  auto new_end = std::ranges::unique(myids);
  myids.erase(new_end.begin(), new_end.end());

  return myids;
}

} // namespace api06
