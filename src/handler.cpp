/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/handler.hpp"

#include <algorithm>

responder::responder(mime::type mt) : mime_type(mt) {}

bool responder::is_available(mime::type mt) const {
  std::vector<mime::type> types = types_available();
  return std::ranges::find(types, mt) != types.end(); // Replace with std::ranges::contains in C++23
}

mime::type responder::resource_type() const { return mime_type; }

std::string responder::extra_response_headers() const { return {}; }

handler::handler(mime::type default_type,
                 http::method methods)
  : mime_type(default_type),
    m_allowed_methods(methods) {}

void handler::set_resource_type(mime::type mt) { mime_type = mt; }

payload_enabled_handler::payload_enabled_handler(mime::type default_type,
                                                 http::method methods) 
  : handler(default_type, methods) {}
