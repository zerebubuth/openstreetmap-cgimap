/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/relation_full_handler.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <fmt/core.h>


namespace api06 {

relation_full_responder::relation_full_responder(mime::type mt_, osm_nwr_id_t id,
                                                 data_selection &w)
    : osm_current_responder(mt_, w) {

  if (sel.select_relations({id}) == 0) {
    throw http::not_found(fmt::format("Relation {:d} was not found.", id));
  }

  check_visibility(id);
  sel.select_nodes_from_relations();
  sel.select_ways_from_relations();
  sel.select_nodes_from_way_nodes();
  sel.select_relations_members_of_relations();
}

void relation_full_responder::check_visibility(osm_nwr_id_t id) {

  switch (sel.check_relation_visibility(id)) {

  case data_selection::non_exist:
    throw http::not_found(fmt::format("Relation {:d} was not found.", id));

  case data_selection::deleted:
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();

  default:
    break;
  }
}

relation_full_handler::relation_full_handler(const request &, osm_nwr_id_t id)
    : id(id) {
  logger::message(
      fmt::format("starting relation/full handler with id = {:d}", id));
}

std::string relation_full_handler::log_name() const { return "relation/full"; }

responder_ptr_t relation_full_handler::responder(data_selection &x) const {
  return std::make_unique<relation_full_responder>(mime_type, id, x);
}

} // namespace api06
