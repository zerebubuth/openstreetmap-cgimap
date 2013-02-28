#include "readonly_pgsql_selection.hpp"
#include "logger.hpp"
#include "quad_tile.hpp"
#include "temp_tables.hpp"
#include "infix_ostream_iterator.hpp"

#include <sstream>
#include <list>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>

using std::set;
using std::stringstream;
using std::list;

// number of nodes to chunk together
#define STRIDE (1000)

namespace {
inline data_selection::visibility_t 
check_table_visibility(pqxx::work &w, osm_id_t id, const char *table) {
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

inline void
insert_results_of(pqxx::work &w, std::stringstream &query, set<osm_id_t> &elems) {
   pqxx::result res = w.exec(query);

   for (pqxx::result::const_iterator itr = res.begin(); 
        itr != res.end(); ++itr) {
      const long int id = (*itr)["id"].as<long int>();
      elems.insert(id);
   }
}
} // anonymous namespace

readonly_pgsql_selection::readonly_pgsql_selection(pqxx::connection &conn)
   : w(conn) {
}

readonly_pgsql_selection::~readonly_pgsql_selection() {
}

void 
readonly_pgsql_selection::write_nodes(output_formatter &formatter) {
   // get all nodes - they already contain their own tags, so
   // we don't need to do anything else.
   logger::message("Fetching nodes");

   formatter.start_element_type(element_type_node, num_nodes());
   // fetch in chunks...
   set<osm_id_t>::iterator prev_itr = sel_nodes.begin();
   size_t chunk_i = 0;
   for (set<osm_id_t>::iterator n_itr = sel_nodes.begin();
        ; ++n_itr, ++chunk_i) {
      bool at_end = n_itr == sel_nodes.end();
      if ((chunk_i >= STRIDE) || at_end) {
         stringstream query;
         query << "select n.id, n.latitude, n.longitude, n.visible, "
            "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp, "
            "n.changeset_id, n.version from current_nodes n where n.id in (";
         std::copy(prev_itr, n_itr, infix_ostream_iterator<osm_id_t>(query, ","));
         query << ")";
         pqxx::result nodes = w.exec(query);
				
         for (pqxx::result::const_iterator itr = nodes.begin(); 
              itr != nodes.end(); ++itr) {
            const long int id = (*itr)["id"].as<long int>();
            pqxx::result tags = w.exec("select k, v from current_node_tags where node_id=" + pqxx::to_string(id));
            formatter.write_node(*itr, tags);
         }

         if (at_end) break;

         chunk_i = 0;
         prev_itr = n_itr;
      }
   }
   formatter.end_element_type(element_type_node);
}

void 
readonly_pgsql_selection::write_ways(output_formatter &formatter) {
   // grab the ways, way nodes and tags
   // way nodes and tags are on a separate connections so that the
   // entire result set can be streamed from a single query.
   logger::message("Fetching ways");

   formatter.start_element_type(element_type_way, num_ways());
   // fetch in chunks...
   set<osm_id_t>::iterator prev_itr = sel_ways.begin();
   size_t chunk_i = 0;
   for (set<osm_id_t>::iterator n_itr = sel_ways.begin();
        ; ++n_itr, ++chunk_i) {
      bool at_end = n_itr == sel_ways.end();
      if ((chunk_i >= STRIDE) || at_end) {
         stringstream query;
         query << "select w.id, w.visible, w.version, w.changeset_id, "
            "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
            "current_ways w where w.id in (";
         std::copy(prev_itr, n_itr, infix_ostream_iterator<osm_id_t>(query, ","));
         query << ")";
         pqxx::result ways = w.exec(query);

         for (pqxx::result::const_iterator itr = ways.begin(); 
              itr != ways.end(); ++itr) {
            const long int id = (*itr)["id"].as<long int>();
            pqxx::result nodes = w.exec("select node_id from current_way_nodes where way_id=" + 
                                        pqxx::to_string(id) + " order by sequence_id asc");
            pqxx::result tags = w.exec("select k, v from current_way_tags where way_id=" + pqxx::to_string(id));
            formatter.write_way(*itr, nodes, tags);
         }
			
         if (at_end) break;

         chunk_i = 0;
         prev_itr = n_itr;
      }
   }
   formatter.end_element_type(element_type_way);
}

void 
readonly_pgsql_selection::write_relations(output_formatter &formatter) {
   logger::message("Fetching relations");

   formatter.start_element_type(element_type_relation, num_relations());
   // fetch in chunks...
   set<osm_id_t>::iterator prev_itr = sel_relations.begin();
   size_t chunk_i = 0;
   for (set<osm_id_t>::iterator n_itr = sel_relations.begin();
        ; ++n_itr, ++chunk_i) {
      bool at_end = n_itr == sel_relations.end();
      if ((chunk_i >= STRIDE) || at_end) {
         stringstream query;
         query << "select r.id, r.visible, r.version, r.changeset_id, "
            "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
            "current_relations r where r.id in (";
         std::copy(prev_itr, n_itr, infix_ostream_iterator<osm_id_t>(query, ","));
         query << ")";
         pqxx::result relations = w.exec(query);
			
         for (pqxx::result::const_iterator itr = relations.begin(); 
              itr != relations.end(); ++itr) {
            const long int id = (*itr)["id"].as<long int>();
            pqxx::result members = w.exec("select member_type, member_id, member_role from "
                                          "current_relation_members where relation_id=" + 
                                          pqxx::to_string(id) + " order by sequence_id asc");
            pqxx::result tags = w.exec("select k, v from current_relation_tags where relation_id=" + pqxx::to_string(id));
            formatter.write_relation(*itr, members, tags);
         }
			
         if (at_end) break;
			
         chunk_i = 0;
         prev_itr = n_itr;
      }
   }
   formatter.end_element_type(element_type_relation);
}

int 
readonly_pgsql_selection::num_nodes() {
   return sel_nodes.size();
}

int 
readonly_pgsql_selection::num_ways() {
   return sel_ways.size();
}

int 
readonly_pgsql_selection::num_relations() {
   return sel_relations.size();
}

data_selection::visibility_t 
readonly_pgsql_selection::check_node_visibility(osm_id_t id) {
   return check_table_visibility(w, id, "node");
}

data_selection::visibility_t 
readonly_pgsql_selection::check_way_visibility(osm_id_t id) {
   return check_table_visibility(w, id, "way");
}

data_selection::visibility_t 
readonly_pgsql_selection::check_relation_visibility(osm_id_t id) {
   return check_table_visibility(w, id, "relation");
}

void
readonly_pgsql_selection::select_nodes(const std::list<osm_id_t> &ids) {
   if (!ids.empty()) {
      stringstream query;
      list<osm_id_t>::const_iterator it;
      
      query << "select id from current_nodes where id IN (";
      std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      
      insert_results_of(w, query, sel_nodes);
   }
}

void
readonly_pgsql_selection::select_ways(const std::list<osm_id_t> &ids) {
   if (!ids.empty()) {
      stringstream query;
      list<osm_id_t>::const_iterator it;
      
      query << "select id from current_ways where id IN (";
      std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      logger::message(query.str());
      
      insert_results_of(w, query, sel_ways);
   }
}

void
readonly_pgsql_selection::select_relations(const std::list<osm_id_t> &ids) {
   if (!ids.empty()) {
      stringstream query;
      list<osm_id_t>::const_iterator it;
      
      query << "select id from current_relations where id IN (";
      std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      
      insert_results_of(w, query, sel_relations);
   }
}

void 
readonly_pgsql_selection::select_nodes_from_bbox(const bbox &bounds, int max_nodes) {
   const set<unsigned int> tiles = 
      tiles_for_area(bounds.minlat, bounds.minlon, 
                     bounds.maxlat, bounds.maxlon);
  
   // hack around problem with postgres' statistics, which was 
   // making it do seq scans all the time on smaug...
   w.exec("set enable_mergejoin=false");
   w.exec("set enable_hashjoin=false");
	
   stringstream query;
   query << "select id from current_nodes where ((";
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
  
   logger::message("Filling sel_nodes from bbox");
   logger::message(query.str());
  
   insert_results_of(w, query, sel_nodes);
}

void 
readonly_pgsql_selection::select_nodes_from_relations() {
   logger::message("Filling sel_nodes (from relations)");
   
   if (!sel_relations.empty()) {
      stringstream query;
      query << "select distinct rm.member_id as id from "
         "current_relation_members rm where rm.member_type='Node'"
         "and rm.relation_id in (";
      std::copy(sel_relations.begin(), sel_relations.end(), infix_ostream_iterator<osm_id_t>(query, ","));	
      query << ")";
      insert_results_of(w, query, sel_nodes);
   }
}

void 
readonly_pgsql_selection::select_ways_from_nodes() {
   logger::message("Filling sel_ways (from nodes)");
   
   if (!sel_nodes.empty()) {
      stringstream query;
      query << "select distinct wn.way_id as id from current_way_nodes wn "
         "where wn.node_id in (";
      std::copy(sel_nodes.begin(), sel_nodes.end(), infix_ostream_iterator<osm_id_t>(query, ","));	
      query << ")";
      insert_results_of(w, query, sel_ways);
   }
}

void 
readonly_pgsql_selection::select_ways_from_relations() {
   logger::message("Filling sel_ways (from relations)");
   
   if (!sel_relations.empty()) {
      stringstream query;
      query << "select distinct rm.member_id as id from "
         "current_relation_members rm where rm.member_type='Way' "
         "and rm.relation_id in (";
      std::copy(sel_relations.begin(), sel_relations.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      insert_results_of(w, query, sel_ways);
   }
}

void 
readonly_pgsql_selection::select_relations_from_ways() {
   logger::message("Filling sel_relations (from ways)");
  
   if (!sel_ways.empty()) {
      stringstream query;
      query << "select distinct relation_id as id from current_relation_members rm where rm.member_type='Way' "
         "and rm.member_id in (";
      std::copy(sel_ways.begin(), sel_ways.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      insert_results_of(w, query, sel_relations);
   }
}

void 
readonly_pgsql_selection::select_nodes_from_way_nodes() {
   if (!sel_ways.empty()) {
      stringstream query;
      query << "select distinct wn.node_id as id from current_way_nodes wn "
         "where wn.way_id in ("; 
      std::copy(sel_ways.begin(), sel_ways.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      insert_results_of(w, query, sel_nodes);
   }
}

void 
readonly_pgsql_selection::select_relations_from_nodes() {
   if (!sel_nodes.empty()) {
      stringstream query;
      query << "select distinct rm.relation_id as id from current_relation_members rm "
         "where rm.member_type='Node' and rm.member_id in (";
      std::copy(sel_nodes.begin(), sel_nodes.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      insert_results_of(w, query, sel_relations);
   }
}

void 
readonly_pgsql_selection::select_relations_from_way_nodes() {
   if (!sel_ways.empty()) {
      stringstream query;
      query << "select distinct relation_id as id from current_relation_members rm "
         "where rm.member_type='Node' and rm.member_id in (select distinct "
         "node_id from current_way_nodes where way_id in (";
      std::copy(sel_ways.begin(), sel_ways.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << "))";
      insert_results_of(w, query, sel_relations);
   }
}

void 
readonly_pgsql_selection::select_relations_from_relations() {
   if (!sel_relations.empty()) {
      stringstream query;
      query << "select distinct relation_id as id from current_relation_members rm "
         "where rm.member_type='Relation' and rm.member_id in (";
      std::copy(sel_relations.begin(), sel_relations.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      insert_results_of(w, query, sel_relations);
   }
}

void 
readonly_pgsql_selection::select_relations_members_of_relations() {
   if (!sel_relations.empty()) {
      stringstream query;
      query << "select distinct rm.member_id as id from current_relation_members rm "
         "where rm.member_type='Relation' and rm.relation_id in (";
      std::copy(sel_relations.begin(), sel_relations.end(), infix_ostream_iterator<osm_id_t>(query, ","));
      query << ")";
      insert_results_of(w, query, sel_relations);
   }
}

readonly_pgsql_selection::factory::factory(pqxx::connection &conn)
   : m_connection(conn) {
}

readonly_pgsql_selection::factory::~factory() {
}

boost::shared_ptr<data_selection> readonly_pgsql_selection::factory::make_selection() {
   return boost::make_shared<readonly_pgsql_selection>(boost::ref(m_connection));
}
