/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef WAY_HPP
#define WAY_HPP

#include "osmobject.hpp"
#include "cgimap/options.hpp"

#include <string>
#include <vector>

#include <fmt/core.h>

namespace api06 {

class Way : public OSMObject {

public:
  Way() = default;

  ~Way() override = default;

  void add_way_node(osm_nwr_signed_id_t waynode) {
    m_way_nodes.emplace_back(waynode);
  }

  void add_way_node(const std::string &waynode) {

    osm_nwr_signed_id_t _waynode = 0;

    try {
      _waynode = std::stol(waynode);
    } catch (std::invalid_argument& e) {
      throw xml_error("Way node is not numeric");
    } catch (std::out_of_range& e) {
      throw xml_error("Way node value is too large");
    }

    if (_waynode == 0) {
      throw xml_error("Way node value may not be 0");
    }

    add_way_node(_waynode);
  }

  const std::vector<osm_nwr_signed_id_t> &nodes() const { return m_way_nodes; }

  bool is_valid(operation op) const {

    switch (op) {

    case operation::op_delete:
      return (is_valid());

    default:
      if (m_way_nodes.empty()) {
        throw http::precondition_failed(
            fmt::format("Way {:d} must have at least one node",
             (has_id() ? id() : 0)));
      }

      if (m_way_nodes.size() > global_settings::get_way_max_nodes()) {
        throw http::bad_request(
             fmt::format(
                 "You tried to add {:d} nodes to way {:d}, however only {:d} are allowed",
             m_way_nodes.size(), (has_id() ? id() : 0), global_settings::get_way_max_nodes()));
      }

      return (is_valid());
    }
  }

  std::string get_type_name() override { return "Way"; }

private:
  std::vector<osm_nwr_signed_id_t> m_way_nodes;
  using OSMObject::is_valid;
};

} // namespace api06

#endif
