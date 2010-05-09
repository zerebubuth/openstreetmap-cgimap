#include "map.hpp"
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <pqxx/pqxx>

#include "temp_tables.hpp"
#include "split_tags.hpp"
#include "cache.hpp"
#include "logger.hpp"

using std::vector;
using std::string;
using std::transform;
using boost::shared_ptr;

void
write_map(pqxx::work &w,
	  output_formatter &formatter,
	  const bbox &bounds) {
  try {
    formatter.start_document(bounds);

    // get all nodes - they already contain their own tags, so
    // we don't need to do anything else.
    logger::message("Fetching nodes");
    int num_nodes = 0;
    formatter.start_element_type(element_type_node, num_nodes);
    pqxx::result nodes = w.exec(
      "select n.id, n.latitude, n.longitude, n.visible, "
      "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp, "
      "n.changeset_id, n.version from current_nodes n join ("
      "select id from tmp_nodes union distinct select wn.node_id "
      "from tmp_ways w join current_way_nodes wn on w.id = wn.id) x "
      "on n.id = x.id");
    for (pqxx::result::const_iterator itr = nodes.begin(); 
	 itr != nodes.end(); ++itr) {
      const long int id = (*itr)["id"].as<long int>();
      pqxx::result tags = w.exec("select k, v from current_node_tags where id=" + pqxx::to_string(id));
      formatter.write_node(*itr, tags);
    }
    formatter.end_element_type(element_type_node);
    
    // grab the ways, way nodes and tags
    // way nodes and tags are on a separate connections so that the
    // entire result set can be streamed from a single query.
    logger::message("Fetching ways");
    int num_ways = 0;
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
    
    logger::message("Fetching relations");
    int num_relations = 0;
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
  
  } catch (const std::exception &e) {
    formatter.error(e);
  }

  formatter.end_document();
}

