/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/osm_responder.hpp"

osm_responder::osm_responder(mime::type mt, std::optional<bbox> b)
    : responder(mt), bounds(b) {}

std::vector<mime::type> osm_responder::types_available() const {
  std::vector<mime::type> types;
  types.push_back(mime::type::application_xml);
#if HAVE_YAJL
  types.push_back(mime::type::application_json);
#endif
  return types;
}

void osm_responder::add_response_header(const std::string &line) {
  extra_headers << line << "\r\n";
}

std::string osm_responder::extra_response_headers() const {
  return extra_headers.str();
}
