/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/relation_relations_handler.hpp"

namespace api06 {

relation_relations_responder::relation_relations_responder(mime::type mt,
                                                           osm_nwr_id_t id,
                                                           data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_relations({id}) > 0 && is_visible(id)) {
    sel.select_relations_from_relations(true);
  }
  else
    sel.drop_relations();
}

bool relation_relations_responder::is_visible(osm_nwr_id_t id) {
  return (!(sel.check_relation_visibility(id) == data_selection::deleted));
}

relation_relations_handler::relation_relations_handler(const request &, osm_nwr_id_t id) : id(id) {}

std::string relation_relations_handler::log_name() const { return "relation/relations"; }

responder_ptr_t relation_relations_handler::responder(data_selection &x) const {
  return std::make_unique<relation_relations_responder>(mime_type, id, x);
}


} // namespace api06
