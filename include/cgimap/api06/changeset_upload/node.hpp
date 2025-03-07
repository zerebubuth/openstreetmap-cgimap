/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef NODE_HPP
#define NODE_HPP

#include "cgimap/api06/changeset_upload/osmobject.hpp"

#include <charconv>
#include <optional>
#include <string>

namespace api06 {

class Node : public OSMObject {

public:
  Node() = default;

  ~Node() override = default;

  double lat() const { return *m_lat; }

  double lon() const { return *m_lon; }

  void set_lat(const std::string &lat) {

    double _lat;

#if !defined(__APPLE__)
    auto [_, ec] = std::from_chars(lat.data(), lat.data() + lat.size(), _lat);

    if (ec == std::errc())
      set_lat(_lat);
    else if (ec == std::errc::invalid_argument)
      throw payload_error("Latitude is not numeric");
    else if (ec == std::errc::result_out_of_range)
      throw payload_error("Latitude value is too large");
    else
      throw payload_error("Unexpected parsing error");

#else

    try {
      _lat = std::stod(lat);
    } catch (std::invalid_argument &e) {
      throw payload_error("Latitude is not numeric");
    } catch (std::out_of_range &e) {
      throw payload_error("Latitude value is too large");
    }
    set_lat(_lat);

#endif
  }

  void set_lon(const std::string &lon) {

    double _lon;

#if !defined(__APPLE__)
    auto [_, ec] = std::from_chars(lon.data(), lon.data() + lon.size(), _lon);

    if (ec == std::errc())
      set_lon(_lon);
    else if (ec == std::errc::invalid_argument)
      throw payload_error("Longitude is not numeric");
    else if (ec == std::errc::result_out_of_range)
      throw payload_error("Longitude value is too large");
    else
      throw payload_error("Unexpected parsing error");

#else

    try {
      _lon = std::stod(lon);
    } catch (std::invalid_argument &e) {
      throw payload_error("Longitude is not numeric");
    } catch (std::out_of_range &e) {
      throw payload_error("Longitude value is too large");
    }

    set_lon(_lon);

#endif
  }

  void set_lat(double lat) {
    if (lat < -90.0 || lat > 90.0)
      throw payload_error("Latitude outside of valid range");
    else if (!std::isfinite(lat))
      throw payload_error("Latitude not a valid finite number");
    m_lat = lat;
  }

  void set_lon(double lon) {
    if (lon < -180.0 || lon > 180.0)
      throw payload_error("Longitude outside of valid range");
    else if (!std::isfinite(lon))
      throw payload_error("Longitude not a valid finite number");
    m_lon = lon;
  }

  bool is_valid(operation op) const {

    if (op == operation::op_delete)
      return (is_valid());

    return (is_valid() && m_lat && m_lon);
  }

  std::string get_type_name() const override { return "Node"; }

  bool operator==(const Node &o) const {
    return (OSMObject::operator==(o) &&
            o.m_lat == m_lat &&
            o.m_lon == m_lon);
  }

private:
  std::optional<double> m_lat;
  std::optional<double> m_lon;
  using OSMObject::is_valid;
};

} // namespace api06

#endif
