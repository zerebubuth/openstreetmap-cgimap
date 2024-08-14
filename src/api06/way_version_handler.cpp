/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/way_version_handler.hpp"
#include "cgimap/http.hpp"

#include <string>

namespace api06 {

way_version_responder::way_version_responder(mime::type mt, 
                                             osm_nwr_id_t id, 
                                             osm_version_t v, 
                                             data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_historical_ways({std::make_pair(id, v)}) == 0) {
     throw http::not_found("");
  }
}

way_version_handler::way_version_handler(const request &, osm_nwr_id_t id, osm_version_t v) : 
  id(id), v(v) {}

std::string way_version_handler::log_name() const { return "way"; }

responder_ptr_t way_version_handler::responder(data_selection &x) const {
  return std::make_unique<way_version_responder>(mime_type, id, v, x);
}

} // namespace api06
