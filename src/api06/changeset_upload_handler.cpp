/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_context.hpp"

#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"
#include "cgimap/api06/changeset_upload/osmchange_xml_input_format.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload_handler.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"

#include <fmt/core.h>

#include <string>

namespace api06 {

changeset_upload_responder::changeset_upload_responder(mime::type mt,
                                                       data_update& upd,
                                                       osm_changeset_id_t changeset,
                                                       const std::string &payload,
                                                       const RequestContext& req_ctx)
    : osm_diffresult_responder(mt) {
  
  if (!req_ctx.user.has_value())
  {
    throw http::server_error("Cannot upload to changeset - no user id");
  }

  OSMChange_Tracking change_tracking{};

  auto changeset_updater = upd.get_changeset_updater(req_ctx, changeset);
  auto node_updater = upd.get_node_updater(req_ctx, change_tracking);
  auto way_updater = upd.get_way_updater(req_ctx, change_tracking);
  auto relation_updater = upd.get_relation_updater(req_ctx, change_tracking);

  changeset_updater->lock_current_changeset(true);

  OSMChange_Handler handler(*node_updater, *way_updater, *relation_updater, changeset);

  // TODO: check HTTP Accept header
  if (mt != mime::type::application_json) {
    OSMChangeXMLParser(handler).process_message(payload);
  }

  // store diffresult for output handling in class osm_diffresult_responder
  m_diffresult = change_tracking.assemble_diffresult();

  const auto new_changes = handler.get_num_changes();

  if (global_settings::get_ratelimiter_upload()) {

    auto max_changes = upd.get_rate_limit(req_ctx.user->id);
    if (new_changes > max_changes)
    {
      logger::message(
          fmt::format(
              "Upload of {} changes by user {} in changeset {} blocked due to rate limiting, max. {} changes allowed",
              new_changes, req_ctx.user->id, changeset, max_changes));
      throw http::too_many_requests("Upload has been blocked due to rate limiting. Please try again later.");
    }
  }

  changeset_updater->update_changeset(new_changes, handler.get_bbox());

  if (global_settings::get_bbox_size_limiter_upload()) {

    auto const cs_bbox = changeset_updater->get_bbox();

    if (!(cs_bbox == bbox_t()))   // valid bbox?
    {
      auto const max_bbox_size = upd.get_bbox_size_limit(req_ctx.user->id);

      if (cs_bbox.linear_size() > max_bbox_size) {

        logger::message(
            fmt::format(
                "Upload of {} changes by user {} in changeset {} blocked due to bbox size limit exceeded, max bbox size {}",
                new_changes, req_ctx.user->id, changeset, max_bbox_size));

        throw http::payload_too_large("Changeset bounding box size limit exceeded.");
      }
    }
  }

  upd.commit();
}

changeset_upload_handler::changeset_upload_handler(const request &,
                                                   osm_changeset_id_t id)
    : payload_enabled_handler(mime::type::unspecified_type,
                              http::method::POST | http::method::OPTIONS),
      id(id) {}

std::string changeset_upload_handler::log_name() const {
  return (fmt::format("changeset/upload {:d}", id));
}

responder_ptr_t changeset_upload_handler::responder(data_selection &) const {
  throw http::server_error(
      "changeset_upload_handler: data_selection unsupported");
}

responder_ptr_t changeset_upload_handler::responder(data_update & upd, 
                                                    const std::string &payload, 
                                                    const RequestContext& req_ctx) const {
  return std::make_unique<changeset_upload_responder>(mime_type, upd, id, payload, req_ctx);
}

bool changeset_upload_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
