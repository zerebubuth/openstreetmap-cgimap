#include "map_handler.hpp"
#include "temp_tables.hpp"
#include "http.hpp"
#include "fcgi_helpers.hpp"
#include "logger.hpp"
#include <boost/format.hpp>
#include <map>

using boost::format;
using std::string;
using std::auto_ptr;
using std::map;

#define MAX_AREA 0.25
#define MAX_NODES 50000

map_responder::map_responder(bbox b, pqxx::work &x)
  : bounds(b), w(x) {
  // create temporary tables of nodes, ways and relations which
  // are in or used by elements in the bbox
  tmp_nodes tn(x, bounds);
  
  // check how many nodes we got
  pqxx::result res = x.exec("select count(*) from tmp_nodes");
  int num_nodes = res[0][0].as<int>();
  // TODO: make configurable parameter?
  if (num_nodes > MAX_NODES) {
    throw http::bad_request((format("You requested too many nodes (limit is %1%). "
				    "Either request a smaller area, or use planet.osm")
			     % MAX_NODES).str());
  }
  
  tmp_ways tw(x);
  tmp_relations tr(x);
}

map_responder::~map_responder() throw() {
}

void
map_responder::write(auto_ptr<output_formatter> formatter) {
  write_map(w, *formatter, bounds);
}

map_handler::map_handler(FCGX_Request &request) 
  : bounds(validate_request(request)) {
}

map_handler::~map_handler() throw() {
}

string
map_handler::log_name() const {
  return (boost::format("map(%1%,%2%,%3%,%4%)") % bounds.minlat % bounds.minlon % bounds.maxlat % bounds.maxlon).str();
}

responder_ptr_t
map_handler::responder(pqxx::work &x) const {
  return responder_ptr_t(new map_responder(bounds, x));
}

formats::format_type
map_handler::format() const {
  return formats::XML;
}

/**
 * Validates an FCGI request, returning the valid bounding box or 
 * throwing an error if there was no valid bounding box.
 */
bbox
map_handler::validate_request(FCGX_Request &request) {
  // check that the REQUEST_METHOD is a GET
  if (fcgi_get_env(request, "REQUEST_METHOD") != "GET") 
    throw http::method_not_allowed("Only the GET method is supported for "
				   "map requests.");

  string decoded = http::urldecode(get_query_string(request));
  const map<string, string> params = http::parse_params(decoded);
  map<string, string>::const_iterator itr = params.find("bbox");

  bbox bounds;
  if ((itr == params.end()) ||
      !bounds.parse(itr->second)) {
    throw http::bad_request("The parameter bbox is required, and must be "
			    "of the form min_lon,min_lat,max_lon,max_lat.");
  }

  // clip the bounding box against the world
  bounds.clip_to_world();

  // check that the bounding box is within acceptable limits. these
  // limits taken straight from the ruby map implementation.
  if (!bounds.valid()) {
    throw http::bad_request("The latitudes must be between -90 and 90, "
			    "longitudes between -180 and 180 and the "
			    "minima must be less than the maxima.");
  }

  // TODO: make configurable parameter?
  if (bounds.area() > MAX_AREA) {
    throw http::bad_request((boost::format("The maximum bbox size is %1%, and your request "
					   "was too large. Either request a smaller area, or use planet.osm")
			     % MAX_AREA).str());
  }

  return bounds;
}

/**
 * writes the temporary nodes and ways, which must have been previously created,
 * to the xml_writer. changesets and users are looked up directly from the 
 * cache rather than joined in SQL.
 */
void
map_responder::write_map(pqxx::work &w,
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
