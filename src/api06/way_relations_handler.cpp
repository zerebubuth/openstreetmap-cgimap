/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/way_relations_handler.hpp"
#include "cgimap/http.hpp"

namespace api06 {

way_relations_responder::way_relations_responder(mime::type mt, osm_nwr_id_t id_,
                                         data_selection &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel.select_ways({id}) > 0 && is_visible()) {
    sel.select_relations_from_ways();
  }
  sel.drop_ways();
}

way_relations_handler::way_relations_handler(request &, osm_nwr_id_t id_) : id(id_) {}

std::string way_relations_handler::log_name() const { return "way/relations"; }

responder_ptr_t way_relations_handler::responder(data_selection &x) const {
  return responder_ptr_t(new way_relations_responder(mime_type, id, x));
}

bool way_relations_responder::is_visible() {
  return (!(sel.check_way_visibility(id) == data_selection::deleted));
}

} // namespace api06
