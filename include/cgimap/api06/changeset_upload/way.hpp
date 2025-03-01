/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
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

  void add_way_nodes(const std::vector<osm_nwr_signed_id_t>& way_nodes) {
    for (auto const wn : way_nodes)
      m_way_nodes.emplace_back(wn);
  }

  void add_way_node(osm_nwr_signed_id_t waynode) {
    if (waynode == 0) {
      throw payload_error("Way node value may not be 0");
    }

    m_way_nodes.emplace_back(waynode);
  }

  void add_way_node(const std::string &waynode) {

    osm_nwr_signed_id_t _waynode = 0;

    auto [_, ec] = std::from_chars(waynode.data(), waynode.data() + waynode.size(), _waynode);

    if (ec == std::errc())
      add_way_node(_waynode);
    else if (ec == std::errc::invalid_argument)
      throw payload_error("Way node is not numeric");
    else if (ec == std::errc::result_out_of_range)
      throw payload_error("Way node value is too large");
    else
      throw payload_error("Unexpected parsing error");
  }

  const std::vector<osm_nwr_signed_id_t> &nodes() const { return m_way_nodes; }

  bool is_valid(operation op) const {

    if (op == operation::op_delete)
      return (is_valid());

    if (m_way_nodes.empty()) {
      throw http::precondition_failed(
          fmt::format("Way {:d} must have at least one node",id(0)));
    }

    auto way_max_nodes = global_settings::get_way_max_nodes();

    if (m_way_nodes.size() > way_max_nodes) {
      throw http::bad_request(
            fmt::format(
                "You tried to add {:d} nodes to way {:d}, however only {:d} are allowed",
            m_way_nodes.size(), id(0), way_max_nodes));
    }

    return (is_valid());
  }

  std::string get_type_name() const override { return "Way"; }

  bool operator==(const Way &o) const {
    return (OSMObject::operator==(o) &&
            o.m_way_nodes == m_way_nodes);
  }

private:
  std::vector<osm_nwr_signed_id_t> m_way_nodes;
  using OSMObject::is_valid;
};

} // namespace api06

#endif
