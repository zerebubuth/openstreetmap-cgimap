#ifndef QUAD_TILE_HPP
#define QUAD_TILE_HPP

#include <set>

std::set<unsigned int> 
tiles_for_area(double minlat, double minlon, 
	       double maxlat, double maxlon);

#endif /* QUAD_TILE_HPP */
