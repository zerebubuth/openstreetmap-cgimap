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
#include "cgimap/api06/changeset_update_handler.hpp"
#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"

#include <fmt/core.h>

#include <string>
#include <memory>


namespace api06 {

changeset_update_responder::changeset_update_responder(
    mime::type mt,
    data_update &upd,
    osm_changeset_id_t changeset_id,
    const std::string &payload,
    const RequestContext& req_ctx)
    : text_responder(mt){

  auto changeset_updater = upd.get_changeset_updater(req_ctx, changeset_id);

  auto tags = ChangesetXMLParser().process_message(payload);

  changeset_updater->api_update_changeset(tags);

  upd.commit();
}

changeset_update_sel_responder::changeset_update_sel_responder(
    mime::type mt,
    data_selection &sel,
    osm_changeset_id_t changeset_id)
    : osm_current_responder(mt, sel),
      sel(sel) {

  sel.select_changesets({changeset_id});
}


changeset_update_handler::changeset_update_handler(const request &, osm_changeset_id_t id)
    : payload_enabled_handler(mime::type::application_xml,
                              http::method::PUT | http::method::OPTIONS),
      id(id) {}

std::string changeset_update_handler::log_name() const {
  return (fmt::format("changeset/update {:d}", id));
}

responder_ptr_t
changeset_update_handler::responder(data_selection &sel) const {
  return std::make_unique<changeset_update_sel_responder>(mime_type, sel, id);
}

responder_ptr_t changeset_update_handler::responder(data_update & upd,
                                                    const std::string &payload,
                                                    const RequestContext& req_ctx) const {
  return std::make_unique<changeset_update_responder>(mime_type, upd, id, payload, req_ctx);
}

bool changeset_update_handler::requires_selection_after_update() const {
  return true;
}

} // namespace api06
