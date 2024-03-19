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

#include "cgimap/api06/changeset_close_handler.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <fmt/core.h>

#include <string>
#include <memory>
#include <optional>

namespace api06 {

changeset_close_responder::changeset_close_responder(mime::type mt, 
                                                     data_update & upd, 
                                                     osm_changeset_id_t changeset, 
                                                     const std::string &, 
                                                     std::optional<osm_user_id_t> user_id)
    : text_responder(mt) {

  auto changeset_updater = upd.get_changeset_updater(changeset, *user_id);
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
                                                   std::optional<osm_user_id_t> user_id) const {
  return std::make_unique<changeset_close_responder>(mime_type, upd, id, payload, user_id);
}

bool changeset_close_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
