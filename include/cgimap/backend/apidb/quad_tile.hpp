/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef QUAD_TILE_HPP
#define QUAD_TILE_HPP

#include "cgimap/types.hpp"

#include <cmath>
#include <vector>


std::vector<tile_id_t> tiles_for_area(double minlat, double minlon, double maxlat,
                                      double maxlon);

/* following functions liberally nicked from TomH's quad_tile
 * library.
 */
constexpr unsigned int xy2tile(unsigned int x, unsigned int y) {
  unsigned int tile = 0;

  for (int i = 15; i >= 0; --i) {
    tile = (tile << 1) | ((x >> i) & 1);
    tile = (tile << 1) | ((y >> i) & 1);
  }

  return tile;
}

inline unsigned int lon2x(double lon) {
  return round((lon + 180.0) * 65535.0 / 360.0);
}

inline unsigned int lat2y(double lat) {
  return round((lat + 90.0) * 65535.0 / 180.0);
}

#endif /* QUAD_TILE_HPP */
