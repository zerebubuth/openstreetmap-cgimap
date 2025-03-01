/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api07/map_handler.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/options.hpp"

#include <fmt/core.h>
#include <map>
#include <ranges>


namespace api07 {

map_responder::map_responder(mime::type mt, bbox b, data_selection &x)
    : osm_current_responder(mt, x, std::optional<bbox>(b)) {
  // create temporary tables of nodes, ways and relations which
  // are in or used by elements in the bbox
  int num_nodes = sel.select_nodes_from_bbox(b, global_settings::get_map_max_nodes());

  if (num_nodes > global_settings::get_map_max_nodes()) {
    throw http::bad_request(
        fmt::format("You requested too many nodes (limit is {:d}). "
                "Either request a smaller area, or use planet.osm"),
            global_settings::get_map_max_nodes());
  }
  // Short-circuit empty areas
  if (num_nodes > 0) {
    sel.select_ways_from_nodes();
    sel.select_nodes_from_way_nodes();
    sel.select_relations_from_ways();
    sel.select_relations_from_nodes();
    sel.select_relations_from_relations();
  }
}

map_handler::map_handler(request &req) : bounds(validate_request(req)) {}

map_handler::map_handler(request &req, int) {}

string map_handler::log_name() const {
  return (fmt::format("map({:.7f},{:.7f},{:.7f},{:.7f})", bounds.minlon,
          bounds.minlat, bounds.maxlon, bounds.maxlat));
}

responder_ptr_t map_handler::responder(data_selection &x) const {
  return std::make_unique<map_responder>(mime_type, bounds, x);
}

/**
 * Validates an FCGI request, returning the valid bounding box or
 * throwing an error if there was no valid bounding box.
 */
bbox map_handler::validate_request(request &req) {
  string decoded = http::urldecode(get_query_string(req));
  const auto params = http::parse_params(decoded);
  auto itr =
    std::ranges::find_if(params,
        [](const auto &p) { return p.first == "bbox"; });

  bbox bounds;
  if ((itr == params.end()) || !bounds.parse(itr->second)) {
    throw http::bad_request("The parameter bbox is required, and must be "
                            "of the form min_lon,min_lat,max_lon,max_lat.");
  }

  // clip the bounding box against the world
  bounds.clip_to_world();

  // check that the bounding box is within acceptable limits. these
  // limits taken straight from the ruby map implementation.
  if (!bounds.valid()) {
    throw http::bad_request("The latitudes must be between -90 and 90, "
                            "longitudes between -180 and 180 and the "
                            "minima must be less than the maxima.");
  }

  if (bounds.area() > global_settings::get_map_area_max()) {
    throw http::bad_request(
         fmt::format("The maximum bbox size is {:d}, and your request "
                       "was too large. Either request a smaller area, or use "
                       "planet.osm", global_settings::get_map_area_max()));
  }

  return bounds;
}

} // namespace api07
