/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/router.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>

namespace match {

match_string::match_string(const char *s) : str(std::string_view(s)) {}

std::pair<match_string::match_type, bool> match_string::match(part_iterator &begin,
                                             const part_iterator &end) const noexcept {
  bool matches = false;
  if (begin != end) {
    auto& bit = *begin;
    matches = (bit == str);
    ++begin;
  }
  return {match_type(), !matches}; // raises error if not matching
}

std::pair<match_osm_id::match_type, bool> match_osm_id::match(part_iterator &begin,
                                             const part_iterator &end) const noexcept {
  if (begin != end) {

    auto& bit = *begin;

    if (bit.end() != std::ranges::find_if(bit, [](unsigned char c) { return !std::isdigit(c); })) {
      return {match_type(), true};
    }

    osm_nwr_id_t x{};
    auto [ptr, ec] = std::from_chars(bit.data(), bit.data() + bit.size(), x);

    if (ec == std::errc() && x > 0) {
      ++begin;
      return {match_type(x), false};
    }
  }
  return {match_type(), true};
}

extern const match_begin root_; // @suppress("Unused variable declaration in file scope")
extern const match_osm_id osm_id_; // @suppress("Unused variable declaration in file scope")
}
