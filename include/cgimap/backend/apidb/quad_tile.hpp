#ifndef QUAD_TILE_HPP
#define QUAD_TILE_HPP

#include <vector>
#include "cgimap/types.hpp"

std::vector<tile_id_t> tiles_for_area(double minlat, double minlon, double maxlat,
                                      double maxlon);

#endif /* QUAD_TILE_HPP */
