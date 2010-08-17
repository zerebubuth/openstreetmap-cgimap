#ifndef MAP_HPP
#define MAP_HPP

#include <pqxx/pqxx>
#include "output_formatter.hpp"
#include "bbox.hpp"

void write_map(pqxx::work &work,
	       output_formatter &formatter,
	       const bbox &bounds);

#endif /* MAP_HPP */
