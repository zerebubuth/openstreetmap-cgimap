#include "writeable_pgsql_selection.hpp"
#include "logger.hpp"
#include "quad_tile.hpp"
#include "temp_tables.hpp"
#include "infix_ostream_iterator.hpp"

#include <set>
#include <sstream>
#include <list>

using std::set;
using std::stringstream;
using std::list;


namespace {
inline data_selection::visibility_t 
check_table_visibility(pqxx::work &w, id_t id, const char *table) {
	stringstream query;
	query << "select visible from current_" << table << "s where id = " << id;
	pqxx::result res = w.exec(query);

	if (res.size() > 0) {
		if (res[0][0].as<bool>()) {
			return data_selection::exists;
		} else {
			return data_selection::deleted;
		}
	} else {
		return data_selection::non_exist;
	}	
}
} // anonymous namespace

writeable_pgsql_selection::writeable_pgsql_selection(pqxx::work &w_)
	: w(w_) {
	w.exec("create temporary table tmp_nodes (id bigint primary key)");
	w.exec("create temporary table tmp_ways (id bigint primary key)");
	w.exec("create temporary table tmp_relations (id bigint primary key)");
}

writeable_pgsql_selection::~writeable_pgsql_selection() {
}

void 
writeable_pgsql_selection::write_nodes(output_formatter &formatter) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  logger::message("Fetching nodes");

  formatter.start_element_type(element_type_node, num_nodes());
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
writeable_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");

  formatter.start_element_type(element_type_way, num_ways());
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
writeable_pgsql_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");

  formatter.start_element_type(element_type_relation, num_relations());
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
writeable_pgsql_selection::num_nodes() {
  pqxx::result res = w.exec("select count(*) from tmp_nodes");
  return res[0][0].as<int>();
}

int 
writeable_pgsql_selection::num_ways() {
  pqxx::result res = w.exec("select count(*) from tmp_ways");
  return res[0][0].as<int>();
}

int 
writeable_pgsql_selection::num_relations() {
  pqxx::result res = w.exec("select count(*) from tmp_relations");
  return res[0][0].as<int>();
}

data_selection::visibility_t 
writeable_pgsql_selection::check_node_visibility(id_t id) {
	return check_table_visibility(w, id, "node");
}

data_selection::visibility_t 
writeable_pgsql_selection::check_way_visibility(id_t id) {
	return check_table_visibility(w, id, "way");
}

data_selection::visibility_t 
writeable_pgsql_selection::check_relation_visibility(id_t id) {
	return check_table_visibility(w, id, "relation");
}

void
writeable_pgsql_selection::select_visible_nodes(const std::list<id_t> &ids) {
	stringstream query;
	list<id_t>::const_iterator it;
	
	query << "insert into tmp_nodes select id from current_nodes where id IN (";
	std::copy(ids.begin(), ids.end(), infix_ostream_iterator<id_t>(query, ","));
	query << ") and visible and id not in (select id from tmp_nodes)";

	w.exec(query);
}

void
writeable_pgsql_selection::select_visible_ways(const std::list<id_t> &ids) {
	stringstream query;
	list<id_t>::const_iterator it;
	
	query << "insert into tmp_ways select id from current_ways where id IN (";
	std::copy(ids.begin(), ids.end(), infix_ostream_iterator<id_t>(query, ","));
	query << ") and visible and id not in (select id from tmp_ways)";

	w.exec(query);
}

void
writeable_pgsql_selection::select_visible_relations(const std::list<id_t> &ids) {
	stringstream query;
	list<id_t>::const_iterator it;
	
	query << "insert into tmp_relations select id from current_relations where id IN (";
	std::copy(ids.begin(), ids.end(), infix_ostream_iterator<id_t>(query, ","));
	query << ") and visible and id not in (select id from tmp_relations)";

	w.exec(query);
}

void 
writeable_pgsql_selection::select_nodes_from_bbox(const bbox &bounds, int max_nodes) {
  const set<unsigned int> tiles = 
		 tiles_for_area(bounds.minlat, bounds.minlon, 
										bounds.maxlat, bounds.maxlon);
  
  // hack around problem with postgres' statistics, which was 
  // making it do seq scans all the time on smaug...
  w.exec("set enable_mergejoin=false");
  w.exec("set enable_hashjoin=false");
	
  stringstream query;
  query << "insert into tmp_nodes "
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
				<< ") and (visible = true) and (id not in (select id from tmp_nodes))"
				<< " limit " << (max_nodes + 1); // limit here as a quick hack to reduce load...

  logger::message("Filling tmp_nodes from bbox");
  logger::message(query.str());

  // assume this throws if it fails?
  w.exec(query);
}

void 
writeable_pgsql_selection::select_nodes_from_relations() {
  logger::message("Filling tmp_nodes (from relations)");

  w.exec("insert into tmp_nodes "
				 "select distinct rm.member_id as id from "
				 "current_relation_members rm join tmp_relations "
				 "tr on rm.id = tr.id where rm.member_type='Node' "
				 "and rm.member_id not in (select id from tmp_nodes)");
}

void 
writeable_pgsql_selection::select_ways_from_nodes() {
  logger::message("Filling tmp_ways (from nodes)");

  w.exec("insert into tmp_ways "
				 "select distinct wn.way_id from current_way_nodes wn "
				 "join tmp_nodes tn on wn.node_id = tn.id "
				 "and wn.way_id not in (select id from tmp_ways)");
}

void 
writeable_pgsql_selection::select_ways_from_relations() {
  logger::message("Filling tmp_ways (from relations)");

  w.exec("insert into tmp_ways "
				 "select distinct rm.member_id as id from "
				 "current_relation_members rm join tmp_relations "
				 "tr on rm.relation_id = tr.id where rm.member_type='Way' "
				 "and rm.member_id not in (select id from tmp_ways");
}

void 
writeable_pgsql_selection::select_relations_from_ways() {
  logger::message("Filling tmp_relations (from ways)");

  w.exec("insert into tmp_relations "
				 "select distinct rm.relation_id from current_relation_members rm where rm.member_type='Way' "
				 "and rm.member_id in (select id from tmp_ways) "
				 "and rm.relation_id not in (select id from tmp_relations)");
}

void 
writeable_pgsql_selection::select_nodes_from_way_nodes() {
  w.exec("insert into tmp_nodes select distinct wn.node_id as id from current_way_nodes wn "
				 "where wn.way_id in (select w.id from tmp_ways w) and wn.node_id not in (select id from tmp_nodes)");
}

void 
writeable_pgsql_selection::select_relations_from_nodes() {
  w.exec("insert into tmp_relations select distinct rm.relation_id from current_relation_members rm "
				 "where rm.member_type='Node' and rm.member_id in (select n.id from tmp_nodes n) "
				 "and rm.relation_id not in (select id from tmp_relations)");
}

void 
writeable_pgsql_selection::select_relations_from_way_nodes() {
  w.exec("insert into tmp_relations select distinct rm.relation_id from current_relation_members rm "
				 "where rm.member_type='Node' and rm.member_id in (select distinct "
				 "node_id from current_way_nodes where way_id in (select id from tmp_ways)) "
				 "and rm.relation_id not in (select id from tmp_relations)");
}

void 
writeable_pgsql_selection::select_relations_from_relations() {
  w.exec("insert into tmp_relations select distinct rm.relation_id from current_relation_members rm "
				 "where rm.member_type='Relation' and rm.member_id in (select id from tmp_relations) "
				 "and rm.relation_id not in (select id from tmp_relations)");
}
