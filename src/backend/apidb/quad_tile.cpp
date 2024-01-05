/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/quad_tile.hpp"
#include <cmath>
#include <set>

using std::set;

/* following functions liberally nicked from TomH's quad_tile
 * library.
 */


std::vector<tile_id_t> tiles_for_area(double minlat, double minlon, double maxlat,
                                   double maxlon) {
  const unsigned int minx = lon2x(minlon);
  const unsigned int maxx = lon2x(maxlon);
  const unsigned int miny = lat2y(minlat);
  const unsigned int maxy = lat2y(maxlat);
  set<tile_id_t> tiles;

  for (tile_id_t x = minx; x <= maxx; x++) {
    for (tile_id_t y = miny; y <= maxy; y++) {
      tiles.insert(xy2tile(x, y));
    }
  }

  return std::vector<tile_id_t>(tiles.begin(), tiles.end());
}
