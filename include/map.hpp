#ifndef MAP_HPP
#define MAP_HPP

#include "writer.hpp"
#include "bbox.hpp"
#include "cache.hpp"
#include "changeset.hpp"
#include <pqxx/pqxx>

/**
 * writes the temporary nodes and ways, which must have been previously created,
 * to the xml_writer. changesets and users are looked up directly from the 
 * cache rather than joined in SQL.
 */
void write_map(pqxx::work &work,
	       xml_writer &writer,
	       const bbox &bounds,
	       cache<long int, changeset> &changeset_cache);

#endif /* MAP_HPP */
