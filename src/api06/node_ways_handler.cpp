/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/node_ways_handler.hpp"

namespace api06 {

node_ways_responder::node_ways_responder(mime::type mt, 
                                         osm_nwr_id_t id,
                                         data_selection &w)
    : osm_current_responder(mt, w) {

  if (sel.select_nodes({ id }) > 0 && is_visible(id)) {
    sel.select_ways_from_nodes();
  }
  sel.drop_nodes();
}

bool node_ways_responder::is_visible(osm_nwr_id_t id) {
  return (!(sel.check_node_visibility(id) == data_selection::deleted));
}

node_ways_handler::node_ways_handler(const request &, osm_nwr_id_t id) : id(id) {}

std::string node_ways_handler::log_name() const { return "node/ways"; }

responder_ptr_t node_ways_handler::responder(data_selection &x) const {
  return std::make_unique<node_ways_responder>(mime_type, id, x);
}


} // namespace api06
