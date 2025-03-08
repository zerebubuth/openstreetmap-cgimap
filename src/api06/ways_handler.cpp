/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/ways_handler.hpp"
#include "cgimap/api06/handler_utils.hpp"
#include "cgimap/http.hpp"

#include <fmt/core.h>
#include <fmt/format.h>

namespace api06 {

ways_responder::ways_responder(mime::type mt, const std::vector<id_version>& ids,
                               data_selection &w)
    : osm_current_responder(mt, w) {

  std::vector<osm_nwr_id_t> current_ids;
  std::vector<osm_edition_t> historic_ids;

  for (id_version idv : ids) {
    if (idv.version) {
      historic_ids.emplace_back(idv.id, *idv.version);
    } else {
      current_ids.push_back(idv.id);
    }
  }

  size_t num_selected = sel.select_ways(current_ids);
  if (!historic_ids.empty()) {
    num_selected += sel.select_historical_ways(historic_ids);
  }

  if (num_selected != ids.size()) {
    throw http::not_found("One or more of the ways were not found.");
  }
}

ways_handler::ways_handler(const request &req) : ids(validate_request(req)) {}

std::string ways_handler::log_name() const {
  return fmt::format("ways?ways={}",fmt::join(ids,","));
}

responder_ptr_t ways_handler::responder(data_selection &x) const {
  return std::make_unique<ways_responder>(mime_type, ids, x);
}

/**
 * Validates an FCGI request, returning the valid list of ids or
 * throwing an error if there was no valid list of way ids.
 */
std::vector<id_version> ways_handler::validate_request(const request &req) {
  std::vector<id_version> myids = parse_id_list_params(req, "ways");

  if (myids.empty()) {
    throw http::bad_request(
      "The parameter ways is required, and must be "
      "of the form ways=ID[vVER][,ID[vVER][,ID[vVER]...]].");
  }

  return myids;
}

} // namespace api06
