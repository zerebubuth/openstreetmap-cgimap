/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_HANDLER_UTILS_HPP
#define API06_HANDLER_UTILS_HPP

#include "cgimap/request.hpp"
#include "cgimap/api06/id_version.hpp"

#include <vector>
#include <string_view>

namespace api06 {

std::vector<id_version> parse_id_list_params(const request &req, std::string_view param_name);
}

#endif /* API06_HANDLER_UTILS_HPP */
