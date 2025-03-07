/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/http.hpp"

#include "cgimap/api06/changeset_close_handler.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/types.hpp"

#include <fmt/core.h>

#include <string>
#include <memory>

namespace api06 {

changeset_close_responder::changeset_close_responder(mime::type mt, 
                                                     data_update & upd, 
                                                     osm_changeset_id_t changeset, 
                                                     const std::string &, 
                                                     const RequestContext& req_ctx)
    : text_responder(mt) {

  auto changeset_updater = upd.get_changeset_updater(req_ctx, changeset);
  changeset_updater->api_close_changeset();
  upd.commit();
}


changeset_close_handler::changeset_close_handler(const request &,
                                                 osm_changeset_id_t id)
    : payload_enabled_handler(mime::type::text_plain,
                              http::method::PUT | http::method::OPTIONS),
      id(id) {}


std::string changeset_close_handler::log_name() const {
  return (fmt::format("changeset/close {:d}", id));
}

responder_ptr_t changeset_close_handler::responder(data_selection &) const {
  throw http::server_error("changeset_close_handler: data_selection unsupported");
}

responder_ptr_t changeset_close_handler::responder(data_update & upd, 
                                                   const std::string &payload, 
                                                   const RequestContext& req_ctx) const {
  return std::make_unique<changeset_close_responder>(mime_type, upd, id, payload, req_ctx);
}

bool changeset_close_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
