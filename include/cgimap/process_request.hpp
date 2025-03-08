/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef PROCESS_REQUEST_HPP
#define PROCESS_REQUEST_HPP

#include "cgimap/request.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/data_update.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/routes.hpp"

#include <string>

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter,
                     const std::string &generator, const routes &route,
                     data_selection::factory& factory,
                     data_update::factory* update_factory);

#endif /* PROCESS_REQUEST_HPP */
