/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <string_view>

/**
 * Contains support for logging.
 */
namespace logger {

/**
 * Initialise logging.
 */
void initialise(const std::string &filename);

/**
 * Log a message.
 */
void message(std::string_view m);
}

#endif /* LOGGER_HPP */
