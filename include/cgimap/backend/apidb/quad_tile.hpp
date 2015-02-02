#ifndef QUAD_TILE_HPP
#define QUAD_TILE_HPP

#include <list>
#include "cgimap/types.hpp"

std::list<osm_id_t> tiles_for_area(double minlat, double minlon, double maxlat,
                                   double maxlon);

#endif /* QUAD_TILE_HPP */
