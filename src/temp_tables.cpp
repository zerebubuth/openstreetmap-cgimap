#include <stdexcept>
#include <sstream>
#include <iostream>
#include "temp_tables.hpp"
#include "quad_tile.hpp"
#include "logger.hpp"

using std::set;
using std::runtime_error;
using std::stringstream;
using std::cerr;
using std::endl;

tmp_nodes::tmp_nodes(pqxx::work &w,
		     const bbox &bounds)
  : work(w) {
  const set<unsigned int> tiles = 
    tiles_for_area(bounds.minlat, bounds.minlon, 
		   bounds.maxlat, bounds.maxlon);
  
  // hack around problem with postgres' statistics, which was 
  // making it do seq scans all the time on smaug...
  w.exec("set enable_mergejoin=false");
  w.exec("set enable_hashjoin=false");

  stringstream query;
  query << "create temporary table tmp_nodes as "
	<< "select id from current_nodes where ((";
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
	<< ") and (visible = true)"
	<< " limit 50001"; // limit here as a quick hack to reduce load...

  logger::message("Creating tmp_nodes");
  logger::message(query.str());

  // assume this throws if it fails?
  work.exec(query);
}

tmp_ways::tmp_ways(pqxx::work &w) 
  : work(w) {
  // we already did this in tmp_nodes, but it can't hurt to do it twice
  work.exec("set enable_mergejoin=false");
  work.exec("set enable_hashjoin=false");

  logger::message("Creating tmp_ways");

  work.exec("create temporary table tmp_ways as "
	    "select distinct wn.id from current_way_nodes wn "
	    "join tmp_nodes tn on wn.node_id = tn.id");
  work.exec("create index tmp_ways_idx on tmp_ways(id)");
}

tmp_relations::tmp_relations(pqxx::work &w) 
  : work(w) {
  // we already did this in tmp_nodes and tmp_ways, but three times is a charm
  work.exec("set enable_mergejoin=false");
  work.exec("set enable_hashjoin=false");

  logger::message("Creating tmp_relations");

  work.exec("create temporary table tmp_relations as "
	    "select distinct rm.id from current_relation_members rm "
	    "where (rm.member_type='Node' and rm.member_id in (select id from tmp_nodes)) "
	    "or (rm.member_type='Way' and rm.member_id in (select id from tmp_ways))");
  work.exec("insert into tmp_relations select id from current_relation_members rm "
	    "where rm.member_type='Relation' and rm.member_id in (select id from tmp_relations) "
	    "and id not in (select id from tmp_relations)");
}
