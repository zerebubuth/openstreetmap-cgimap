/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/way_full_handler.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <fmt/core.h>

namespace api06 {

way_full_responder::way_full_responder(mime::type mt, 
                                       osm_nwr_id_t id,
                                       data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_ways({id}) == 0) {
    throw http::not_found(fmt::format("Way {:d} was not found.", id));
  }
  check_visibility(id);
  sel.select_nodes_from_way_nodes();
}

void way_full_responder::check_visibility(osm_nwr_id_t id) {
  switch (sel.check_way_visibility(id)) {

  case data_selection::non_exist:
    throw http::not_found(fmt::format("Way {:d} was not found.", id));

  case data_selection::deleted:
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();

  default:
    break;
  }
}

way_full_handler::way_full_handler(const request &, osm_nwr_id_t id) : id(id) {
  logger::message(fmt::format("starting way/full handler with id = {:d}", id));
}

std::string way_full_handler::log_name() const { return "way/full"; }

responder_ptr_t way_full_handler::responder(data_selection &x) const {
  return std::make_unique<way_full_responder>(mime_type, id, x);
}

} // namespace api06
