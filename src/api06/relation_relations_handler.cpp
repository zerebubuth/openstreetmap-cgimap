/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/relation_relations_handler.hpp"
#include "cgimap/http.hpp"

namespace api06 {

relation_relations_responder::relation_relations_responder(mime::type mt, osm_nwr_id_t id_,
                                         data_selection &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel.select_relations({id}) > 0 && is_visible()) {
    sel.select_relations_from_relations(true);
  }
  else
    sel.drop_relations();
}

relation_relations_handler::relation_relations_handler(request &, osm_nwr_id_t id_) : id(id_) {}

std::string relation_relations_handler::log_name() const { return "relation/relations"; }

responder_ptr_t relation_relations_handler::responder(data_selection &x) const {
  return responder_ptr_t(new relation_relations_responder(mime_type, id, x));
}

bool relation_relations_responder::is_visible() {
  return (!(sel.check_relation_visibility(id) == data_selection::deleted));
}

} // namespace api06
