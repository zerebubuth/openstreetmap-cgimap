#include "cgimap/api06/relations_handler.hpp"
#include "cgimap/api06/handler_utils.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include <sstream>

using std::stringstream;
using std::list;
using std::string;

namespace api06 {

relations_responder::relations_responder(mime::type mt, list<osm_id_t> ids_,
                                         factory_ptr &s_)
    : osm_current_responder(mt, s_), ids(ids_) {
  size_t num_selected = sel->select_relations(ids_);
  if (num_selected != ids.size()) {
    throw http::not_found("One or more of the relations were not found.");
  }
}

relations_responder::~relations_responder() {}

relations_handler::relations_handler(request &req)
    : ids(validate_request(req)) {}

relations_handler::~relations_handler() {}

std::string relations_handler::log_name() const {
  stringstream msg;
  msg << "relations?relations=";
  std::copy(ids.begin(), ids.end(),
            infix_ostream_iterator<osm_id_t>(msg, ", "));
  return msg.str();
}

responder_ptr_t relations_handler::responder(factory_ptr &x) const {
  return responder_ptr_t(new relations_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or
 * throwing an error if there was no valid list of node ids.
 */
list<osm_id_t> relations_handler::validate_request(request &req) {
  list<osm_id_t> myids = parse_id_list_params(req, "relations");

  if (myids.size() < 1) {
    throw http::bad_request("The parameter relations is required, and must be "
                            "of the form relations=id[,id[,id...]].");
  }

  return myids;
}

} // namespace api06
