#ifndef MAP_HPP
#define MAP_HPP

#include "writer.hpp"
#include "bbox.hpp"
#include <pqxx/pqxx>

void write_map(pqxx::work &work,
	       xml_writer &writer,
	       const bbox &bounds);

#endif /* MAP_HPP */
