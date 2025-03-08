/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_NODES_HANDLER_HPP
#define API06_NODES_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include "cgimap/api06/id_version.hpp"

#include <string>
#include <vector>

namespace api06 {

class nodes_responder : public osm_current_responder {
public:
  nodes_responder(mime::type, const std::vector<id_version>&, data_selection &);
};

class nodes_handler : public handler {
public:
  explicit nodes_handler(const request &req);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  std::vector<id_version> ids;

  static std::vector<id_version> validate_request(const request &req);
};

} // namespace api06

#endif /* API06_NODE_HANDLER_HPP */
