/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_WAY_HANDLER_HPP
#define API06_WAY_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class way_responder : public osm_current_responder {
public:
  way_responder(mime::type, osm_nwr_id_t, data_selection &);

private:
  void check_visibility(osm_nwr_id_t);
};

class way_handler : public handler {
public:
  way_handler(const request &req, osm_nwr_id_t id);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  osm_nwr_id_t id;
};

} // namespace api06

#endif /* API06_WAY_HANDLER_HPP */
