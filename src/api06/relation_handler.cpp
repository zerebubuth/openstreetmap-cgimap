/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/relation_handler.hpp"
#include "cgimap/http.hpp"

namespace api06 {

relation_responder::relation_responder(mime::type mt, osm_nwr_id_t id_,
                                       data_selection &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel.select_relations({id}) == 0) {
    throw http::not_found(fmt::format("Relation {:d} was not found.", id));
  }
  check_visibility();
}

relation_handler::relation_handler(request &, osm_nwr_id_t id_) : id(id_) {}

std::string relation_handler::log_name() const { return "relation"; }

responder_ptr_t relation_handler::responder(data_selection &x) const {
  return responder_ptr_t(new relation_responder(mime_type, id, x));
}

void relation_responder::check_visibility() {
  if (sel.check_relation_visibility(id) == data_selection::deleted) {
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();
  }
}

} // namespace api06
