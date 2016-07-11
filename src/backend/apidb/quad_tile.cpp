#include "cgimap/backend/apidb/quad_tile.hpp"
#include <cmath>
#include <set>

using std::set;

/* following functions liberally nicked from TomH's quad_tile
 * library.
 */
inline unsigned int xy2tile(unsigned int x, unsigned int y) {
  unsigned int tile = 0;
  int i;

  for (i = 15; i >= 0; --i) {
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
