/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unistd.h>
#include <memory>

#include "cgimap/logger.hpp"

namespace logger {

static std::unique_ptr<std::ostream> stream;
static pid_t pid;

void initialise(const std::string &filename) {
  stream = std::make_unique<std::ofstream>(filename, std::ios_base::out | std::ios_base::app);
  pid = getpid();
}

void message(std::string_view m) {
  if (stream) {
    time_t now = time(nullptr);
    *stream << "[" << std::put_time( std::gmtime( &now ), "%FT%T") << " #" << pid << "] " << m
            << std::endl;
  }
}

}
