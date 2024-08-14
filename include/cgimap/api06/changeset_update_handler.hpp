/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_CHANGESET_UPDATE_HANDLER_HPP
#define API06_CHANGESET_UPDATE_HANDLER_HPP

#include <string>

#include "cgimap/text_responder.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class changeset_update_responder : public text_responder {
public:
  changeset_update_responder(mime::type,
                             data_update &,
                             osm_changeset_id_t id_,
                             const std::string & payload,
                             const RequestContext& req_ctx);
};

class changeset_update_sel_responder : public osm_current_responder {
public:
  changeset_update_sel_responder(mime::type,
                                 data_selection & sel,
                                 osm_changeset_id_t id_);
private:
  data_selection& sel;
};

class changeset_update_handler : public payload_enabled_handler {
public:
  changeset_update_handler(const request &req, osm_changeset_id_t id);

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

#endif /* API06_CHANGESET_UPDATE_HANDLER_HPP */
