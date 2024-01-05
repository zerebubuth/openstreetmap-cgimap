/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/utils.hpp"

void check_postgres_version(pqxx::connection_base &conn) {
  auto version = conn.server_version();
  if (version < 90400) {
    throw std::runtime_error("Expected Postgres version 9.4+, currently installed version "
      + std::to_string(version));
  }
}
