/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/way_history_handler.hpp"
#include "cgimap/http.hpp"

#include <string>
#include <memory>

namespace api06 {

way_history_responder::way_history_responder(mime::type mt, osm_nwr_id_t id, data_selection &w)
  : osm_current_responder(mt, w) {

  if (sel.select_ways_with_history({id}) == 0) {
    throw http::not_found("");
  }
}

way_history_handler::way_history_handler(const request &, osm_nwr_id_t id) : id(id) {}

std::string way_history_handler::log_name() const { return "way/history"; }

responder_ptr_t way_history_handler::responder(data_selection &w) const {
  return std::make_unique<way_history_responder>(mime_type, id, w);
}

} // namespace api06
