/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/relation_history_handler.hpp"
#include "cgimap/http.hpp"

namespace api06 {

relation_history_responder::relation_history_responder(mime::type mt, osm_nwr_id_t id_, data_selection &w_)
  : osm_current_responder(mt, w_), id(id_) {

  if (sel.select_relations_with_history({id}) == 0) {
    throw http::not_found("");
  }
}

relation_history_handler::relation_history_handler(request &, osm_nwr_id_t id_) : id(id_) {}

std::string relation_history_handler::log_name() const { return "relation/history"; }

responder_ptr_t relation_history_handler::responder(data_selection &w) const {
  return responder_ptr_t(new relation_history_responder(mime_type, id, w));
}

} // namespace api06
