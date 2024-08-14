/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/relation_version_handler.hpp"
#include "cgimap/http.hpp"

namespace api06 {

relation_version_responder::relation_version_responder(mime::type mt, osm_nwr_id_t id,
                                       osm_version_t v, data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_historical_relations({std::make_pair(id, v)}) == 0) {
     throw http::not_found("");
  }
}

relation_version_handler::relation_version_handler(const request &, osm_nwr_id_t id, osm_version_t v) : id(id), v(v) {}

std::string relation_version_handler::log_name() const { return "relation"; }

responder_ptr_t relation_version_handler::responder(data_selection &x) const {
  return std::make_unique<relation_version_responder>(mime_type, id, v, x);
}

} // namespace api06
