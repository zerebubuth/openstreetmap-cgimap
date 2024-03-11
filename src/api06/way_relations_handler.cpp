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

#include <string>
#include <memory>

namespace api06 {

way_relations_responder::way_relations_responder(mime::type mt, 
                                                 osm_nwr_id_t id,
                                                 data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_ways({id}) > 0 && is_visible(id)) {
    sel.select_relations_from_ways();
  }
  sel.drop_ways();
}

bool way_relations_responder::is_visible(osm_nwr_id_t id) {
  return (!(sel.check_way_visibility(id) == data_selection::deleted));
}


way_relations_handler::way_relations_handler(const request &, osm_nwr_id_t id) : id(id) {}

std::string way_relations_handler::log_name() const { return "way/relations"; }

responder_ptr_t way_relations_handler::responder(data_selection &x) const {
  return std::make_unique<way_relations_responder>(mime_type, id, x);
}


} // namespace api06
