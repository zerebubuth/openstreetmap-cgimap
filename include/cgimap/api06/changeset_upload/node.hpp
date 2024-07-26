/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef NODE_HPP
#define NODE_HPP

#include "cgimap/api06/changeset_upload/osmobject.hpp"

#include <charconv>
#include <optional>

namespace api06 {

class Node : public OSMObject {

public:
  Node() = default;

  ~Node() override = default;

  double lat() const { return *m_lat; }

  double lon() const { return *m_lon; }

  void set_lat(const std::string &lat) {

    double _lat;

    auto [_, ec] = std::from_chars(lat.data(), lat.data() + lat.size(), _lat);

    if (ec == std::errc())
      set_lat(_lat);
    else if (ec == std::errc::invalid_argument)
      throw xml_error("Latitude is not numeric");
    else if (ec == std::errc::result_out_of_range)
      throw xml_error("Latitude value is too large");
  }

  void set_lon(const std::string &lon) {

    double _lon;

    auto [_, ec] = std::from_chars(lon.data(), lon.data() + lon.size(), _lon);

    if (ec == std::errc())
      set_lon(_lon);
    else if (ec == std::errc::invalid_argument)
      throw xml_error("Longitude is not numeric");
    else if (ec == std::errc::result_out_of_range)
      throw xml_error("Longitude value is too large");
  }

  void set_lat(double lat) {
    if (lat < -90 || lat > 90)
      throw xml_error("Latitude outside of valid range");
    else if (!std::isfinite(lat))
      throw xml_error("Latitude not a valid finite number");
    m_lat = lat;
  }

  void set_lon(double lon) {
    if (lon < -180 || lon > 180)
      throw xml_error("Longitude outside of valid range");
    else if (!std::isfinite(lon))
      throw xml_error("Longitude not a valid finite number");
    m_lon = lon;
  }

  bool is_valid(operation op) const {

    switch (op) {

    case operation::op_delete:
      return (is_valid());
    default:
      return (is_valid() && m_lat && m_lon);
    }
  }

  std::string get_type_name() override { return "Node"; }

private:
  std::optional<double> m_lat;
  std::optional<double> m_lon;
  using OSMObject::is_valid;
};

} // namespace api06

#endif
