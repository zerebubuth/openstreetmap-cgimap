/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_CHANGESET_UPLOAD_HANDLER_HPP
#define API06_CHANGESET_UPLOAD_HANDLER_HPP

#include <string>

#include "cgimap/handler.hpp"
#include "cgimap/osm_diffresult_responder.hpp"
#include "cgimap/request.hpp"

struct RequestContext;

namespace api06 {

class changeset_upload_responder : public osm_diffresult_responder {
public:
  changeset_upload_responder(mime::type, data_update &, osm_changeset_id_t,
                             const std::string &,
                             const RequestContext& req_ctx);
};

class changeset_upload_handler : public payload_enabled_handler {
public:
  changeset_upload_handler(const request &req, osm_changeset_id_t id);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

  responder_ptr_t responder(data_update &,
                            const std::string &payload,
                            const RequestContext& req_ctx) const override;
  bool requires_selection_after_update() const override;

private:
  osm_changeset_id_t id;
};

} // namespace api06

#endif /* API06_CHANGESET_UPLOAD_HANDLER_HPP */
