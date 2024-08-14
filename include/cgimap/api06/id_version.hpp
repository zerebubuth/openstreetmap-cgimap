/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_ID_VERSION_HPP
#define API06_ID_VERSION_HPP

#include "cgimap/types.hpp"
#include <optional>

#include <fmt/core.h>
#include <fmt/format.h>

namespace api06 {

struct id_version {
  id_version() : id(0) {}
  explicit id_version(osm_nwr_id_t i) : id(i) {}
  id_version(osm_nwr_id_t i, uint32_t v) : id(i), version(v) {}

  inline bool operator==(const id_version &other) const {
    return (id == other.id) && (version == other.version);
  }
  inline bool operator!=(const id_version &other) const {
    return !operator==(other);
  }
  inline bool operator<(const id_version &other) const {
    return (id < other.id) ||
      ((id == other.id) && bool(version) &&
       (!bool(other.version) || (*version < *other.version)));
  }
  inline bool operator<=(const id_version &other) const {
    return operator<(other) || operator==(other);
  }
  inline bool operator>(const id_version &other) const {
    return !operator<=(other);
  }
  inline bool operator>=(const id_version &other) const {
    return !operator<(other);
  }

  osm_nwr_id_t id;
  std::optional<uint32_t> version;
};

} // namespace api06

template <> struct fmt::formatter<api06::id_version> {
  template <typename FormatContext>
  auto format(const api06::id_version& r, FormatContext& ctx) const -> decltype(ctx.out()) {
    if (r.version) {
      return fmt::format_to(ctx.out(), "{:d}v{:d}", r.id, *r.version);
    }

    return fmt::format_to(ctx.out(), "{:d}", r.id);
  }
  constexpr auto parse(const format_parse_context& ctx) const { return ctx.begin(); }
};


#endif /* API06_ID_VERSION_HPP */
