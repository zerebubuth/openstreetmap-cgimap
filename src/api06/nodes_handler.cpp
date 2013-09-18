#include "api06/nodes_handler.hpp"
#include "api06/handler_utils.hpp"
#include "http.hpp"
#include "logger.hpp"
#include "infix_ostream_iterator.hpp"

#include <sstream>

using std::stringstream;
using std::list;
using std::string;

namespace api06 {

nodes_responder::nodes_responder(mime::type mt, list<osm_id_t> ids_, data_selection &w_)
	: osm_responder(mt, w_), ids(ids_) {

	if (sel.select_nodes(ids_) != ids.size()) {
		throw http::not_found("One or more of the nodes were not found.");			
	}
}

nodes_responder::~nodes_responder() {
}

nodes_handler::nodes_handler(FCGX_Request &request) 
	: ids(validate_request(request)) {
}

nodes_handler::~nodes_handler() {
}

std::string 
nodes_handler::log_name() const {
  stringstream msg;
  msg << "nodes?nodes=";
  std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(msg, ", "));
  return msg.str();
}

responder_ptr_t 
nodes_handler::responder(data_selection &x) const {
	return responder_ptr_t(new nodes_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or 
 * throwing an error if there was no valid list of node ids.
 */
list<osm_id_t>
nodes_handler::validate_request(FCGX_Request &request) {
  list<osm_id_t> ids = parse_id_list_params(request, "nodes");

  if (ids.size() < 1) {
    throw http::bad_request("The parameter nodes is required, and must be "
                            "of the form nodes=id[,id[,id...]].");
  }
  
  return ids;
}

} // namespace api06 
