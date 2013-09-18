#include "api06/relations_handler.hpp"
#include "api06/handler_utils.hpp"
#include "http.hpp"
#include "logger.hpp"
#include "infix_ostream_iterator.hpp"

#include <sstream>

using std::stringstream;
using std::list;
using std::string;

namespace api06 {

relations_responder::relations_responder(mime::type mt, list<osm_id_t> ids_, data_selection &s_)
  : osm_responder(mt, s_), ids(ids_) {
  if ( sel.select_relations(ids_) != ids.size()) {
    throw http::not_found("One or more of the relations were not found.");		
  }
}

relations_responder::~relations_responder() {
}

relations_handler::relations_handler(FCGX_Request &request) 
	: ids(validate_request(request)) {
}

relations_handler::~relations_handler() {
}

std::string 
relations_handler::log_name() const {
  stringstream msg;
  msg << "relations?relations=";
  std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(msg, ", "));
  return msg.str();
}

responder_ptr_t 
relations_handler::responder(data_selection &x) const {
	return responder_ptr_t(new relations_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or 
 * throwing an error if there was no valid list of node ids.
 */
list<osm_id_t>
relations_handler::validate_request(FCGX_Request &request) {
  list <osm_id_t> myids = parse_id_list_params(request, "relations");
  
  if (myids.size() < 1) {
    throw http::bad_request("The parameter relations is required, and must be "
                            "of the form relations=id[,id[,id...]].");
  }
  
  return myids;
}

} // namespace api06

