/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/types.hpp"
#include "cgimap/http.hpp"
#include "cgimap/api06/changeset_create_handler.hpp"
#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"

#include <string>

namespace api06 {

changeset_create_responder::changeset_create_responder(mime::type mt, 
                                                       data_update & upd, 
                                                       const std::string &payload,
                                                       const RequestContext& req_ctx)
    : text_responder(mt) {

  osm_changeset_id_t changeset = 0;

  auto changeset_updater = upd.get_changeset_updater(req_ctx, changeset);
  auto tags = ChangesetXMLParser().process_message(payload);
  changeset = changeset_updater->api_create_changeset(tags);

  output_text = std::to_string(changeset);
  upd.commit();
}

changeset_create_handler::changeset_create_handler(const request &)
    : payload_enabled_handler(mime::type::text_plain,
                              http::method::PUT | http::method::OPTIONS) {}

std::string changeset_create_handler::log_name() const {
  return "changeset/create";
}

responder_ptr_t
changeset_create_handler::responder(data_selection &) const {
  throw http::server_error("changeset_create_handler: data_selection unsupported");
}

responder_ptr_t changeset_create_handler::responder(data_update & upd, 
                                                    const std::string &payload, 
                                                    const RequestContext& req_ctx) const {
  return std::make_unique<changeset_create_responder>(mime_type, upd, payload, req_ctx);
}

bool changeset_create_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
