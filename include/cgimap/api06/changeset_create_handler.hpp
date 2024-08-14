/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_CHANGESET_CREATE_HANDLER_HPP
#define API06_CHANGESET_CREATE_HANDLER_HPP

#include <string>

#include "cgimap/text_responder.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class changeset_create_responder : public text_responder {
public:
  changeset_create_responder(mime::type, 
                             data_update &,
                             const std::string &,
                             const RequestContext& req_ctx);
};

class changeset_create_handler : public payload_enabled_handler {
public:
  explicit changeset_create_handler(const request &req);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

  responder_ptr_t responder(data_update &,
                            const std::string &payload,
                            const RequestContext& req_ctx) const override;
  bool requires_selection_after_update() const override;
};

} // namespace api06

#endif /* API06_CHANGESET_CREATE_HANDLER_HPP */
