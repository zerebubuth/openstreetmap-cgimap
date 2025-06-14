/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/osm_responder.hpp"

osm_responder::osm_responder(mime::type mt, std::optional<bbox> b)
    : responder(mt), bounds(b) {}

std::vector<mime::type> osm_responder::types_available() const {
  return {mime::type::application_xml, mime::type::application_json};
}
