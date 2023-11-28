/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/way_handler.hpp"
#include "cgimap/http.hpp"

#include <fmt/core.h>

namespace api06 {

way_responder::way_responder(mime::type mt, osm_nwr_id_t id_, data_selection &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel.select_ways({id}) == 0) {
    throw http::not_found(fmt::format("Way {:d} was not found.", id));
  }
  check_visibility();
}

way_handler::way_handler(request &, osm_nwr_id_t id_) : id(id_) {}

std::string way_handler::log_name() const { return "way"; }

responder_ptr_t way_handler::responder(data_selection &x) const {
  return responder_ptr_t(new way_responder(mime_type, id, x));
}

void way_responder::check_visibility() {
  if (sel.check_way_visibility(id) == data_selection::deleted) {
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();
  }
}

} // namespace api06
