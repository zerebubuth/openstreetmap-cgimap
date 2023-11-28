/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/node_relations_handler.hpp"
#include "cgimap/http.hpp"

namespace api06 {

node_relations_responder::node_relations_responder(mime::type mt, osm_nwr_id_t id_,
                                         data_selection &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel.select_nodes({id}) > 0 && is_visible()) {
    sel.select_relations_from_nodes();
  }
  sel.drop_nodes();
}

node_relations_handler::node_relations_handler(request &, osm_nwr_id_t id_) : id(id_) {}

std::string node_relations_handler::log_name() const { return "node/relations"; }

responder_ptr_t node_relations_handler::responder(data_selection &x) const {
  return responder_ptr_t(new node_relations_responder(mime_type, id, x));
}

bool node_relations_responder::is_visible() {
  return (!(sel.check_node_visibility(id) == data_selection::deleted));
}

} // namespace api06
