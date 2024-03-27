/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OAUTH2_HPP
#define OAUTH2_HPP

#include <memory>
#include <string>

#include "cgimap/data_selection.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"

namespace oauth2 {
  std::optional<osm_user_id_t> validate_bearer_token(const request &req, data_selection& selection, bool& allow_api_write);
}

#endif
