/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef UTIL_TIME_HPP
#define UTIL_TIME_HPP

#include <string>
#include <chrono>

// parse a time string (ISO 8601 - YYYY-MM-DDTHH:MM:SSZ)
std::chrono::system_clock::time_point parse_time(const std::string &);

#endif /* UTIL_TIME_HPP */
