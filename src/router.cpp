/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/router.hpp"

namespace match {

error::error() : std::runtime_error("error!") {}

match_string::match_string(const std::string &s) : str(s) {}

match_string::match_string(const char *s) : str(s) {}

std::pair<match_string::match_type, bool> match_string::match(part_iterator &begin,
                                             const part_iterator &end) const noexcept {
  bool matches = false;
  if (begin != end) {
    const std::string& bit = *begin;
    matches = (bit == str);
    ++begin;
  }
  return {match_type(), !matches}; // raises error if not matching
}

std::pair<match_osm_id::match_type, bool> match_osm_id::match(part_iterator &begin,
                                             const part_iterator &end) const noexcept {
  if (begin != end) {
    try {
      const std::string& bit = *begin;
      // note that osm_nwr_id_t is actually unsigned, so we lose a bit of
      // precision here, but it's OK since IDs are postgres 'bigint' types
      // which are also signed, so element 2^63 is unlikely to exist.
      auto x = std::stol(bit);
      if (x > 0) {
        ++begin;
        return {match_type(x), false};
      }
    } catch (std::exception &e) {
      return {match_type(), true};
    }
  }
  return {match_type(), true};
}

extern const match_begin root_; // @suppress("Unused variable declaration in file scope")
extern const match_osm_id osm_id_; // @suppress("Unused variable declaration in file scope")
}
