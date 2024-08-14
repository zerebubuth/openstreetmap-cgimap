/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/node_version_handler.hpp"
#include "cgimap/http.hpp"

namespace api06 {

node_version_responder::node_version_responder(mime::type mt, 
                                               osm_nwr_id_t id, 
                                               osm_version_t v, 
                                               data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_historical_nodes({std::make_pair(id, v)}) == 0) {
     throw http::not_found("");
  }
}

node_version_handler::node_version_handler(const request &, osm_nwr_id_t id, osm_version_t v) : 
  id(id), v(v) {}

std::string node_version_handler::log_name() const { return "node"; }

responder_ptr_t node_version_handler::responder(data_selection &w) const {
  return std::make_unique<node_version_responder>(mime_type, id, v, w);
}

} // namespace api06
