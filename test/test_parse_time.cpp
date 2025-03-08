/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/time.hpp"

#include <ctime>
#include <stdexcept>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE("Parse time", "[time]")
{
  struct tm dat{};
  dat.tm_year = 2015 - 1900;
  dat.tm_mon = 8 - 1;
  dat.tm_mday = 31;
  dat.tm_hour = 23;
  dat.tm_min = 40;
  dat.tm_sec = 10;

  std::time_t tm_t = timegm(&dat);

  CHECK(std::chrono::system_clock::from_time_t(tm_t) == parse_time("2015-08-31T23:40:10Z"));

  REQUIRE_THROWS_AS(parse_time("2015-08-31T23:40:10"), std::runtime_error);
}
