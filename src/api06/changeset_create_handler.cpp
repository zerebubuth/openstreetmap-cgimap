/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"

#include "cgimap/api06/changeset_create_handler.hpp"
#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"


#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <string>
#include <optional>

namespace api06 {

changeset_create_responder::changeset_create_responder(mime::type mt, 
                                                       data_update & upd, 
                                                       const std::string &payload,
                                                       std::optional<osm_user_id_t> user_id)
    : text_responder(mt) {

  osm_changeset_id_t changeset = 0;

  auto changeset_updater = upd.get_changeset_updater(changeset, *user_id);
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
                                                    std::optional<osm_user_id_t> user_id) const {
  return std::make_unique<changeset_create_responder>(mime_type, upd, payload, user_id);
}

bool changeset_create_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
