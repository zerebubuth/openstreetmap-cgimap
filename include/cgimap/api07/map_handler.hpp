/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API07_MAP_HANDLER_HPP
#define API07_MAP_HANDLER_HPP

#include "cgimap/bbox.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

#ifndef ENABLE_API07
#error This file should not be included unless experimental API 0.7 features are enabled.
#endif /* ENABLE_API07 */

namespace api07 {

class map_responder : public osm_current_responder {
public:
  map_responder(mime::type, bbox, data_selection &);
};

class map_handler : public handler {
public:
  explicit map_handler(request &req);
  map_handler(request &req, int tile_id);
  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  bbox bounds;
  bbox validate_request(request &req);
};

} // namespace api07

#endif /* API07_MAP_HANDLER_HPP */
