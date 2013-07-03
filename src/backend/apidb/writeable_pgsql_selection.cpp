#include "backend/apidb/writeable_pgsql_selection.hpp"
#include "backend/apidb/apidb.hpp"
#include "logger.hpp"
#include "backend/apidb/quad_tile.hpp"
#include "infix_ostream_iterator.hpp"

#include <set>
#include <sstream>
#include <list>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>

using std::set;
using std::stringstream;
using std::list;

namespace pqxx {
template<> struct string_traits<list<osm_id_t> >
{
   static const char *name() { return "list<osm_id_t>"; }
   static bool has_null() { return false; }
   static bool is_null(const list<osm_id_t> &) { return false; }
   static stringstream null()
   {
      internal::throw_null_conversion(name());
      // No, dear compiler, we don't need a return here.                                                                                                
      throw 0;
   }
   static void from_string(const char Str[], list<osm_id_t> &Obj) {
   }
   static std::string to_string(const list<osm_id_t> &ids) {
      stringstream ostr;
      ostr << "{";
      std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(ostr, ","));
      ostr << "}";
      return ostr.str();
   }
};
}

namespace {
inline data_selection::visibility_t 
check_table_visibility(pqxx::work &w, osm_id_t id, const std::string &prepared_name) {
   pqxx::result res = w.prepared(prepared_name)(id).exec();
   
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

void extract_elem(const pqxx::result::tuple &row, element_info &elem) {
   elem.id = row["id"].as<osm_id_t>();
   elem.version = row["version"].as<int>();
   elem.changeset = row["changeset_id"].as<osm_id_t>();
   elem.visible = row["visible"].as<bool>();
   elem.uid = 0;
   elem.display_name = boost::none;
   elem.timestamp = row["timestamp"].c_str();
}

void extract_tags(const pqxx::result &res, tags_t &tags) {
   tags.clear();
   for (pqxx::result::const_iterator itr = res.begin();
        itr != res.end(); ++itr) {
      tags.push_back(std::make_pair(std::string((*itr)["k"].c_str()),
                                    std::string((*itr)["v"].c_str())));
   }
}

void extract_nodes(const pqxx::result &res, nodes_t &nodes) {
   nodes.clear();
   for (pqxx::result::const_iterator itr = res.begin();
        itr != res.end(); ++itr) {
      nodes.push_back((*itr)[0].as<osm_id_t>());
   }
}

element_type type_from_name(const char *name) {
   element_type type;

   switch (name[0]) {
   case 'N':
   case 'n':
      type = element_type_node;
      break;

   case 'W':
   case 'w':
      type = element_type_way;
      break;

   case 'R':
   case 'r':
      type = element_type_relation;
      break;
   }

   return type;
}

void extract_members(const pqxx::result &res, members_t &members) {
   member_info member;
   members.clear();
   for (pqxx::result::const_iterator itr = res.begin();
        itr != res.end(); ++itr) {
      member.type = type_from_name((*itr)["member_type"].c_str());
      member.ref = (*itr)["member_id"].as<osm_id_t>();
      member.role = (*itr)["member_role"].c_str();
      members.push_back(member);
   }
}

} // anonymous namespace

writeable_pgsql_selection::writeable_pgsql_selection(pqxx::connection &conn)
   : w(conn) {
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
  element_info elem;
  double lon, lat;
  tags_t tags;

  formatter.start_element_type(element_type_node, num_nodes());
  pqxx::result nodes = w.prepared("extract_nodes").exec();
  for (pqxx::result::const_iterator itr = nodes.begin(); 
       itr != nodes.end(); ++itr) {
     extract_elem(*itr, elem);
     lon = double((*itr)["longitude"].as<int64_t>()) / (SCALE);
     lat = double((*itr)["latitude"].as<int64_t>()) / (SCALE);
     extract_tags(w.prepared("extract_node_tags")(elem.id).exec(), tags);
     formatter.write_node(elem, lon, lat, tags);
  }
  formatter.end_element_type(element_type_node);
}

void 
writeable_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");
  element_info elem;
  nodes_t nodes;
  tags_t tags;

  formatter.start_element_type(element_type_way, num_ways());
  pqxx::result ways = w.prepared("extract_ways").exec();
  for (pqxx::result::const_iterator itr = ways.begin(); 
       itr != ways.end(); ++itr) {
     extract_elem(*itr, elem);
     extract_nodes(w.prepared("extract_way_nds")(elem.id).exec(), nodes);
     extract_tags(w.prepared("extract_way_tags")(elem.id).exec(), tags);
     formatter.write_way(elem, nodes, tags);
  }
  formatter.end_element_type(element_type_way);
}

void 
writeable_pgsql_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");
  element_info elem;
  members_t members;
  tags_t tags;

  formatter.start_element_type(element_type_relation, num_relations());
  pqxx::result relations = w.prepared("extract_relations").exec();
  for (pqxx::result::const_iterator itr = relations.begin(); 
       itr != relations.end(); ++itr) {
     extract_elem(*itr, elem);
     extract_members(w.prepared("extract_relation_members")(elem.id).exec(), members);
     extract_tags(w.prepared("extract_relation_tags")(elem.id).exec(), tags);
     formatter.write_relation(elem, members, tags);
  }
  formatter.end_element_type(element_type_relation);
}

int 
writeable_pgsql_selection::num_nodes() {
   pqxx::result res = w.prepared("count_nodes").exec();
   // count should always return a single row, right?
   return res[0][0].as<int>();
}

int 
writeable_pgsql_selection::num_ways() {
   pqxx::result res = w.prepared("count_ways").exec();
   // count should always return a single row, right?
   return res[0][0].as<int>();
}

int 
writeable_pgsql_selection::num_relations() {
   pqxx::result res = w.prepared("count_relations").exec();
   // count should always return a single row, right?
   return res[0][0].as<int>();
}

data_selection::visibility_t 
writeable_pgsql_selection::check_node_visibility(osm_id_t id) {
   return check_table_visibility(w, id, "visible_node");
}

data_selection::visibility_t 
writeable_pgsql_selection::check_way_visibility(osm_id_t id) {
   return check_table_visibility(w, id, "visible_way");
}

data_selection::visibility_t 
writeable_pgsql_selection::check_relation_visibility(osm_id_t id) {
   return check_table_visibility(w, id, "visible_relation");
}

void
writeable_pgsql_selection::select_nodes(const std::list<osm_id_t> &ids) {
   w.prepared("add_nodes_list")(ids).exec();
}

void
writeable_pgsql_selection::select_ways(const std::list<osm_id_t> &ids) {
   w.prepared("add_ways_list")(ids).exec();
}

void
writeable_pgsql_selection::select_relations(const std::list<osm_id_t> &ids) {
   w.prepared("add_relations_list")(ids).exec();
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

   w.prepared("nodes_from_relations").exec();
}

void 
writeable_pgsql_selection::select_ways_from_nodes() {
  logger::message("Filling tmp_ways (from nodes)");

  w.prepared("ways_from_nodes").exec();
}

void 
writeable_pgsql_selection::select_ways_from_relations() {
   logger::message("Filling tmp_ways (from relations)");
   w.prepared("ways_from_relations").exec();
}

void 
writeable_pgsql_selection::select_relations_from_ways() {
  logger::message("Filling tmp_relations (from ways)");

  w.prepared("relations_from_ways").exec();
}

void 
writeable_pgsql_selection::select_nodes_from_way_nodes() {
   w.prepared("nodes_from_way_nodes").exec();
}

void 
writeable_pgsql_selection::select_relations_from_nodes() {
   w.prepared("relations_from_nodes").exec();
}

void 
writeable_pgsql_selection::select_relations_from_relations() {
   w.prepared("relations_from_relations").exec();
}

void 
writeable_pgsql_selection::select_relations_members_of_relations() {
   w.prepared("relation_members_of_relations").exec();
}

writeable_pgsql_selection::factory::factory(pqxx::connection &conn)
   : m_connection(conn) {
   logger::message("Preparing prepared statements.");

   // selecting node, way and relation visibility information
   m_connection.prepare("visible_node",     "select visible from current_nodes     where id = $1")("bigint");
   m_connection.prepare("visible_way",      "select visible from current_ways      where id = $1")("bigint");
   m_connection.prepare("visible_relation", "select visible from current_relations where id = $1")("bigint");

   // extraction functions for getting the data back out when the
   // selection set has been built up.
   m_connection.prepare("extract_nodes",
      "select n.id, n.latitude, n.longitude, n.visible, "
      "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp, "
      "n.changeset_id, n.version from current_nodes n join tmp_nodes x "
      "on n.id = x.id");
   m_connection.prepare("extract_ways",
      "select w.id, w.visible, w.version, w.changeset_id, "
      "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
      "current_ways w join tmp_ways tw on w.id=tw.id");
   m_connection.prepare("extract_relations",
      "select r.id, r.visible, r.version, r.changeset_id, "
      "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
      "current_relations r join tmp_relations x on x.id=r.id");
   m_connection.prepare("extract_way_nds", 
      "select node_id from current_way_nodes where way_id=$1 "
      "order by sequence_id asc")
      ("bigint");
   m_connection.prepare("extract_relation_members",
      "select member_type, member_id, member_role from current_relation_members "
      "where relation_id=$1 order by sequence_id asc")
      ("bigint");
   m_connection.prepare("extract_node_tags",     "select k, v from current_node_tags     where node_id=$1")    ("bigint");
   m_connection.prepare("extract_way_tags",      "select k, v from current_way_tags      where way_id=$1")     ("bigint");
   m_connection.prepare("extract_relation_tags", "select k, v from current_relation_tags where relation_id=$1")("bigint");

   // counting things which are in the working set
   m_connection.prepare("count_nodes",     "select count(*) from tmp_nodes");
   m_connection.prepare("count_ways",      "select count(*) from tmp_ways");
   m_connection.prepare("count_relations", "select count(*) from tmp_relations");

   // selecting a set of nodes as a list
   m_connection.prepare("add_nodes_list",
      "insert into tmp_nodes select id from current_nodes where "
      "id = ANY($1) and id not in (select id from tmp_nodes)")
      ("bigint[]");
   m_connection.prepare("add_ways_list",
      "insert into tmp_ways select id from current_ways where id = ANY($1) "
      "and id not in (select id from tmp_ways)")
      ("bigint[]");
   m_connection.prepare("add_relations_list",
      "insert into tmp_relations select id from current_relations where id = ANY($1) "
      "and id not in (select id from tmp_relations)")
      ("bigint[]");

   // queries for filling elements which are used as members in relations
   m_connection.prepare("nodes_from_relations",
      "insert into tmp_nodes select distinct rm.member_id as id from "
      "current_relation_members rm join tmp_relations tr on "
      "rm.relation_id = tr.id where rm.member_type='Node' "
      "and rm.member_id not in (select id from tmp_nodes)");
   m_connection.prepare("ways_from_relations",
      "insert into tmp_ways select distinct rm.member_id as id from "
      "current_relation_members rm join tmp_relations tr on "
      "rm.relation_id = tr.id where rm.member_type='Way' "
      "and rm.member_id not in (select id from tmp_ways)");
   m_connection.prepare("relation_members_of_relations",
      "insert into tmp_relations select distinct rm.member_id as id from "
      "current_relation_members rm join tmp_relations tr on "
      "rm.relation_id = tr.id where rm.member_type='Relation' "
      "and rm.member_id not in (select id from tmp_relations)");

   // select ways which use nodes already in the working set
   m_connection.prepare("ways_from_nodes",
      "insert into tmp_ways select distinct wn.way_id from "
      "current_way_nodes wn join tmp_nodes tn on wn.node_id "
      "= tn.id and wn.way_id not in (select id from tmp_ways)");
   // select nodes used by ways already in the working set
   m_connection.prepare("nodes_from_way_nodes",
      "insert into tmp_nodes select distinct wn.node_id as id from "
      "current_way_nodes wn where wn.way_id in (select w.id from "
      "tmp_ways w) and wn.node_id not in (select id from tmp_nodes)");

   // selecting relations which have members which are already in
   // the working set.
   m_connection.prepare("relations_from_nodes",
      "insert into tmp_relations select distinct rm.relation_id from "
      "current_relation_members rm where rm.member_type='Node' and "
      "rm.member_id in (select n.id from tmp_nodes n) and rm.relation_id "
      "not in (select id from tmp_relations)");
   m_connection.prepare("relations_from_ways",
      "insert into tmp_relations select distinct rm.relation_id from "
      "current_relation_members rm where rm.member_type='Way' "
      "and rm.member_id in (select id from tmp_ways) "
      "and rm.relation_id not in (select id from tmp_relations)");
   m_connection.prepare("relations_from_relations",
      "insert into tmp_relations select distinct rm.relation_id from "
      "current_relation_members rm where rm.member_type='Relation' and "
      "rm.member_id in (select id from tmp_relations) and rm.relation_id "
      "not in (select id from tmp_relations)");
}

writeable_pgsql_selection::factory::~factory() {
}

boost::shared_ptr<data_selection> writeable_pgsql_selection::factory::make_selection() {
   return boost::make_shared<writeable_pgsql_selection>(boost::ref(m_connection));
}
