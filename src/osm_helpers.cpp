#include "osm_helpers.hpp"
#include "logger.hpp"
#include "quad_tile.hpp"
#include "temp_tables.hpp"

#include <set>
#include <sstream>

using std::set;
using std::stringstream;

namespace osm_helpers {

void
write_tmp_nodes(pqxx::work &w, output_formatter &formatter, int num_nodes) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  logger::message("Fetching nodes");

  formatter.start_element_type(element_type_node, num_nodes);
  pqxx::result nodes = w.exec(
      "select n.id, n.latitude, n.longitude, n.visible, "
      "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp, "
      "n.changeset_id, n.version from current_nodes n join tmp_nodes x "
      "on n.id = x.id");
  for (pqxx::result::const_iterator itr = nodes.begin(); 
       itr != nodes.end(); ++itr) {
    const long int id = (*itr)["id"].as<long int>();
    pqxx::result tags = w.exec("select k, v from current_node_tags where id=" + pqxx::to_string(id));
    formatter.write_node(*itr, tags);
  }
  formatter.end_element_type(element_type_node);
}

void
write_tmp_ways(pqxx::work &w, output_formatter &formatter, int num_ways) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");

  formatter.start_element_type(element_type_way, num_ways);
  pqxx::result ways = w.exec(
      "select w.id, w.visible, w.version, w.changeset_id, "
      "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
      "current_ways w join tmp_ways tw on w.id=tw.id where w.visible = true");
  for (pqxx::result::const_iterator itr = ways.begin(); 
       itr != ways.end(); ++itr) {
    const long int id = (*itr)["id"].as<long int>();
    pqxx::result nodes = w.exec("select node_id from current_way_nodes where id=" + 
				pqxx::to_string(id) + " order by sequence_id asc");
    pqxx::result tags = w.exec("select k, v from current_way_tags where id=" + pqxx::to_string(id));
    formatter.write_way(*itr, nodes, tags);
  }
  formatter.end_element_type(element_type_way);
}

void
write_tmp_relations(pqxx::work &w, output_formatter &formatter, int num_relations) {
  logger::message("Fetching relations");

  formatter.start_element_type(element_type_relation, num_relations);
  pqxx::result relations = w.exec(
      "select r.id, r.visible, r.version, r.changeset_id, "
      "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
      "current_relations r join tmp_relations x on x.id=r.id where r.visible = true");
  for (pqxx::result::const_iterator itr = relations.begin(); 
       itr != relations.end(); ++itr) {
    const long int id = (*itr)["id"].as<long int>();
    pqxx::result members = w.exec("select member_type, member_id, member_role from "
				  "current_relation_members where id=" + 
				  pqxx::to_string(id) + " order by sequence_id asc");
    pqxx::result tags = w.exec("select k, v from current_relation_tags where id=" + pqxx::to_string(id));
    formatter.write_relation(*itr, members, tags);
  }
  formatter.end_element_type(element_type_relation);
}

int
num_nodes(pqxx::work &x) {
  pqxx::result res = x.exec("select count(*) from tmp_nodes");
  return res[0][0].as<int>();
}

int
num_ways(pqxx::work &x) {
  pqxx::result res = x.exec("select count(*) from tmp_ways");
  return res[0][0].as<int>();
}

int
num_relations(pqxx::work &x) {
  pqxx::result res = x.exec("select count(*) from tmp_relations");
  return res[0][0].as<int>();
}

void
create_tmp_nodes_from_bbox(pqxx::work &w, const bbox &bounds, int max_nodes) {
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
	<< " limit " << (max_nodes + 1); // limit here as a quick hack to reduce load...

  logger::message("Creating tmp_nodes");
  logger::message(query.str());

  // assume this throws if it fails?
  w.exec(query);
}

void
create_tmp_ways_from_nodes(pqxx::work &work) {
  logger::message("Creating tmp_ways");

  work.exec("create temporary table tmp_ways as "
	    "select distinct wn.id from current_way_nodes wn "
	    "join tmp_nodes tn on wn.node_id = tn.id");
  work.exec("create index tmp_ways_idx on tmp_ways(id)");
}

void
create_tmp_relations_from_ways(pqxx::work &work) {
  logger::message("Creating tmp_relations");

  work.exec("create temporary table tmp_relations as "
	    "select distinct id from current_relation_members rm where rm.member_type='Way' "
	    "and rm.member_id in (select id from tmp_ways)");
  work.exec("create index tmp_relations_idx on tmp_relations(id)");
}

void
insert_tmp_nodes_from_way_nodes(pqxx::work &work) {
  work.exec("insert into tmp_nodes select distinct wn.node_id from current_way_nodes wn "
	    "where wn.id in (select w.id from tmp_ways w) and wn.node_id not in (select "
	    "id from tmp_nodes)");
}

void
insert_tmp_relations_from_nodes(pqxx::work &work) {
  work.exec("insert into tmp_relations select distinct rm.id from current_relation_members rm "
	    "where rm.member_type='Node' and rm.member_id in (select n.id from tmp_nodes n) "
	    "and rm.id not in (select id from tmp_relations)");
}

void
insert_tmp_relations_from_way_nodes(pqxx::work &work) {
  work.exec("insert into tmp_relations select distinct id from current_relation_members rm "
	    "where rm.member_type='Node' and rm.member_id in (select distinct "
	    "node_id from current_way_nodes where id in (select id from tmp_ways)) "
	    "and id not in (select id from tmp_relations)");
}

void
insert_tmp_relations_from_relations(pqxx::work &work) {
  work.exec("insert into tmp_relations select distinct id from current_relation_members rm "
	    "where rm.member_type='Relation' and rm.member_id in (select id from tmp_relations) "
	    "and id not in (select id from tmp_relations)");
}

} // end namespace osm_helpers
