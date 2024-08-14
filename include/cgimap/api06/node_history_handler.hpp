/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_NODE_HISTORY_HANDLER_HPP
#define API06_NODE_HISTORY_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class node_history_responder : public osm_current_responder {
public:
  node_history_responder(mime::type, osm_nwr_id_t, data_selection &);
};

class node_history_handler : public handler {
public:
  node_history_handler(const request &, osm_nwr_id_t);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &) const override;

private:
  osm_nwr_id_t id;
};

} // namespace api06

#endif /* API06_NODE_HISTORY_HANDLER_HPP */
