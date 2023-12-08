/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>

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
void message(const std::string &m);
}

#endif /* LOGGER_HPP */
