/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/relation_handler.hpp"
#include "cgimap/http.hpp"

#include <fmt/core.h>

namespace api06 {

relation_responder::relation_responder(mime::type mt, osm_nwr_id_t id,
                                       data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_relations({id}) == 0) {
    throw http::not_found(fmt::format("Relation {:d} was not found.", id));
  }
  check_visibility(id);
}

void relation_responder::check_visibility(osm_nwr_id_t id) {
  if (sel.check_relation_visibility(id) == data_selection::deleted) {
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();
  }
}

relation_handler::relation_handler(const request &, osm_nwr_id_t id) : id(id) {}

std::string relation_handler::log_name() const { return "relation"; }

responder_ptr_t relation_handler::responder(data_selection &x) const {
  return std::make_unique<relation_responder>(mime_type, id, x);
}


} // namespace api06
