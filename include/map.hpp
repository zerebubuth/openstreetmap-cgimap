#ifndef MAP_HPP
#define MAP_HPP

#include "writer.hpp"
#include "bbox.hpp"
#include <mysql++/mysql++.h>

void write_map(mysqlpp::Connection &con,
	       mysqlpp::Connection &con2,
	       xml_writer &writer,
	       const bbox &bounds);

#endif /* MAP_HPP */
