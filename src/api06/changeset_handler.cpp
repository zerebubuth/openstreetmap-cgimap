/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/changeset_handler.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"

#include <fmt/core.h>

using std::vector;

namespace api06 {

changeset_responder::changeset_responder(mime::type mt, 
                                         osm_changeset_id_t id,
                                         bool include_discussion,
                                         data_selection &w)
  : osm_current_responder(mt, w) {

  if (sel.select_changesets({id}) == 0) {
    throw http::not_found(fmt::format("Changeset {:d} was not found.", id));
  }

  if (include_discussion) {
    sel.select_changeset_discussions();
  }
}


changeset_handler::changeset_handler(const request &req, osm_changeset_id_t id)
  : id(id), include_discussion(false) {

  std::string decoded = http::urldecode(get_query_string(req));
  const auto params = http::parse_params(decoded);
  auto itr =
    std::find_if(params.begin(), params.end(),
         [](const std::pair<std::string, std::string> &header) {
               return (header.first == "include_discussion");
  });

  include_discussion = (itr != params.end());
}


std::string changeset_handler::log_name() const { return "changeset"; }

responder_ptr_t changeset_handler::responder(data_selection &w) const {
  return std::make_unique<changeset_responder>(mime_type, id, include_discussion, w);
}

} // namespace api06
