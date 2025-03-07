/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/changeset_download_handler.hpp"
#include "cgimap/http.hpp"

#include <fmt/core.h>

namespace api06 {

changeset_download_responder::changeset_download_responder(mime::type mt,
                                                           osm_changeset_id_t id,
                                                           data_selection &w)
  : osmchange_responder(mt, w) {

  if (sel.select_changesets({id}) == 0) {
    throw http::not_found(fmt::format("Changeset {:d} was not found.", id));
  }
  sel.select_historical_by_changesets({id});
}

changeset_download_handler::changeset_download_handler(const request &, osm_changeset_id_t id)
  : id(id) {
}

std::string changeset_download_handler::log_name() const { return "changeset/download"; }

responder_ptr_t changeset_download_handler::responder(data_selection &w) const {
  return std::make_unique<changeset_download_responder>(mime_type, id, w);
}

} // namespace api06
