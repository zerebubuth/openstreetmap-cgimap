/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/nodes_handler.hpp"
#include "cgimap/api06/handler_utils.hpp"
#include "cgimap/http.hpp"

#include <fmt/core.h>
#include <fmt/format.h>


namespace api06 {

nodes_responder::nodes_responder(mime::type mt, const std::vector<id_version> &ids,
                                 data_selection &w)
    : osm_current_responder(mt, w) {

  std::vector<osm_nwr_id_t> current_ids;
  std::vector<osm_edition_t> historic_ids;

  for (const auto &idv : ids) {
    if (idv.version) {
      historic_ids.emplace_back(idv.id, *idv.version);
    } else {
      current_ids.push_back(idv.id);
    }
  }

  size_t num_selected = sel.select_nodes(current_ids);
  if (!historic_ids.empty()) {
    num_selected += sel.select_historical_nodes(historic_ids);
  }

  if (num_selected != ids.size()) {
    throw http::not_found("One or more of the nodes were not found.");
  }
}

nodes_handler::nodes_handler(const request &req) : ids(validate_request(req)) {}

std::string nodes_handler::log_name() const {
  return fmt::format("nodes?nodes={}",fmt::join(ids,","));
}

responder_ptr_t nodes_handler::responder(data_selection &x) const {
  return std::make_unique<nodes_responder>(mime_type, ids, x);
}

/**
 * Validates an FCGI request, returning the valid list of ids or
 * throwing an error if there was no valid list of node ids.
 */
std::vector<id_version> nodes_handler::validate_request(const request &req) {
  std::vector<id_version> ids = parse_id_list_params(req, "nodes");

  if (ids.empty()) {
    throw http::bad_request(
      "The parameter nodes is required, and must be "
      "of the form nodes=ID[vVER][,ID[vVER][,ID[vVER]...]].");
  }

  return ids;
}

} // namespace api06
