#include "map_handler.hpp"
#include "temp_tables.hpp"
#include "http.hpp"
#include "fcgi_helpers.hpp"
#include "osm_helpers.hpp"
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
  osm_helpers::create_tmp_nodes_from_bbox(w, bounds, MAX_NODES);

  // check how many nodes we got
  int num_nodes = osm_helpers::num_nodes(w);
  // TODO: make configurable parameter?
  if (num_nodes > MAX_NODES) {
    throw http::bad_request((format("You requested too many nodes (limit is %1%). "
				    "Either request a smaller area, or use planet.osm")
			     % MAX_NODES).str());
  }

  osm_helpers::create_tmp_ways_from_nodes(w);
  osm_helpers::insert_tmp_nodes_from_way_nodes(w);
  osm_helpers::create_tmp_relations_from_ways(w);
  osm_helpers::insert_tmp_relations_from_nodes(w);
  osm_helpers::insert_tmp_relations_from_way_nodes(w);
  osm_helpers::insert_tmp_relations_from_relations(w);
}

map_responder::~map_responder() throw() {
}

void
map_responder::write(auto_ptr<output_formatter> formatter) {
  write_map(w, *formatter, bounds);
}

map_handler::map_handler(FCGX_Request &request) 
  : bounds(validate_request(request)),
    output_format(parse_format(request)) {
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
  return output_format;
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
    formatter.start_document();
    formatter.write_bounds(bounds);

    int num_nodes = osm_helpers::num_nodes(w);
    int num_ways = osm_helpers::num_ways(w);
    int num_relations = osm_helpers::num_relations(w);

    osm_helpers::write_tmp_nodes(w, formatter, num_nodes);
    osm_helpers::write_tmp_ways(w, formatter, num_ways);
    osm_helpers::write_tmp_relations(w, formatter, num_relations);
  
  } catch (const std::exception &e) {
    formatter.error(e);
  }

  formatter.end_document();
}

formats::format_type 
map_handler::parse_format(FCGX_Request &request) {
  string request_path = get_request_path(request);
  if (request_path.substr(request_path.size() - 5) == string(".json")) {
    return formats::JSON;
  } else {
    return formats::XML;
  }
}
