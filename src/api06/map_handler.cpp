#include "api06/map_handler.hpp"
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

namespace api06 {

map_responder::map_responder(mime::type mt, bbox b, data_selection &x)
  : osm_responder(mt, x, boost::optional<bbox>(b)) {
  // create temporary tables of nodes, ways and relations which
  // are in or used by elements in the bbox
  int num_nodes = sel.select_nodes_from_bbox(b, MAX_NODES);

  // TODO: make configurable parameter?
  if (num_nodes > MAX_NODES) {
    throw http::bad_request((format("You requested too many nodes (limit is %1%). "
				    "Either request a smaller area, or use planet.osm")
			     % MAX_NODES).str());
  }
  // Short-circuit empty areas
  if (num_nodes > 0) {
    sel.select_ways_from_nodes();
    sel.select_nodes_from_way_nodes();
    sel.select_relations_from_ways();
    sel.select_relations_from_nodes();
    sel.select_relations_from_relations();
  }

  // map calls typically have a Content-Disposition header saying that
  // what's coming back is an attachment.
  add_response_header("Content-Disposition: attachment; filename=\"map.osm\"");
}

map_responder::~map_responder() {
}

map_handler::map_handler(FCGX_Request &request) 
  : bounds(validate_request(request)) {
}

map_handler::~map_handler() {
}

string
map_handler::log_name() const {
  return (boost::format("map(%1%,%2%,%3%,%4%)") % bounds.minlon % bounds.minlat % bounds.maxlon % bounds.maxlat).str();
}

responder_ptr_t
map_handler::responder(data_selection &x) const {
  return responder_ptr_t(new map_responder(mime_type, bounds, x));
}

/**
 * Validates an FCGI request, returning the valid bounding box or 
 * throwing an error if there was no valid bounding box.
 */
bbox
map_handler::validate_request(FCGX_Request &request) {
  string decoded = http::urldecode(get_query_string(request));
  const map<string, string> params = http::parse_params(decoded);
  map<string, string>::const_iterator itr = params.find("bbox");

  bbox bounds;
  if ((itr == params.end()) ||
      !bounds.parse(itr->second)) {
    throw http::bad_request("The parameter bbox is required, and must be "
			    "of the form min_lon,min_lat,max_lon,max_lat.");
  }

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

} // namespace api06
