#include <stdexcept>
#include "temp_tables.hpp"
#include "quad_tile.hpp"

using std::set;
using std::runtime_error;

tmp_nodes::tmp_nodes(mysqlpp::Connection &c,
		     const bbox &bounds)
  : con(c) {
  const set<unsigned int> tiles = 
    tiles_for_area(bounds.minlat, bounds.minlon, 
		   bounds.maxlat, bounds.maxlon);
  
  mysqlpp::Query query = con.query();
  query << "create temporary table `tmp_nodes` engine=memory "
	<< "select id from `current_nodes` where ((";
  unsigned int first_id = 0, last_id = 0;
  for (set<unsigned int>::const_iterator itr = tiles.begin();
       itr != tiles.end(); ++itr) {
    if (first_id == 0) {
      last_id = first_id = *itr;
    } else if (*itr == last_id + 1) {
      ++last_id;
    } else {
      if (last_id == first_id) {
	query << "tile = " << last_id << " or ";
      } else {
	query << "tile between " << first_id 
	      << " and " << last_id << " or ";
      }
      last_id = first_id = *itr;
    }
  }
  if (last_id == first_id) {
    query << "tile = " << last_id << ") ";
  } else {
    query << "tile between " << first_id 
	  << " and " << last_id << ") ";
  }
  query << "and latitude between " << int(bounds.minlat * SCALE) 
	<< " and " << int(bounds.maxlat * SCALE)
	<< " and longitude between " << int(bounds.minlon * SCALE) 
	<< " and " << int(bounds.maxlon * SCALE)
	<< ") and (visible = 1)";
  
  if (!query.exec()) {
    throw runtime_error("couldn't create temporary table for node IDs");
  }
}

tmp_nodes::~tmp_nodes() {
  mysqlpp::Query query = con.query();
  query << "drop table `tmp_nodes`";
  query.exec();
}

tmp_ways::tmp_ways(mysqlpp::Connection &c) 
  : con(c) {
  mysqlpp::Query query = con.query();
  
  query << "create temporary table `tmp_ways` engine=memory "
	<< "select distinct wn.id from `current_way_nodes` wn "
	<< "join `tmp_nodes` tn on wn.node_id = tn.id";
  
  if (!query.exec()) {
    throw runtime_error("couldn't create temporary table for way IDs");
  }      
}

tmp_ways::~tmp_ways() {
  mysqlpp::Query query = con.query();
  query << "drop table `tmp_ways`";
  query.exec();
}    

