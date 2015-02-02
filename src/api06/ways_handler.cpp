#include "cgimap/api06/ways_handler.hpp"
#include "cgimap/api06/handler_utils.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include <sstream>

using std::stringstream;
using std::list;
using std::string;

namespace api06 {

ways_responder::ways_responder(mime::type mt, list<osm_id_t> ids_,
                               factory_ptr &w_)
    : osm_current_responder(mt, w_), ids(ids_) {
  size_t num_selected = sel->select_ways(ids_);
  if (num_selected != ids.size()) {
    throw http::not_found("One or more of the ways were not found.");
  }
}

ways_responder::~ways_responder() {}

ways_handler::ways_handler(request &req) : ids(validate_request(req)) {}

ways_handler::~ways_handler() {}

std::string ways_handler::log_name() const {
  stringstream msg;
  msg << "ways?ways=";
  std::copy(ids.begin(), ids.end(),
            infix_ostream_iterator<osm_id_t>(msg, ", "));
  return msg.str();
}

responder_ptr_t ways_handler::responder(factory_ptr &x) const {
  return responder_ptr_t(new ways_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or
 * throwing an error if there was no valid list of way ids.
 */
list<osm_id_t> ways_handler::validate_request(request &req) {
  list<osm_id_t> myids = parse_id_list_params(req, "ways");

  if (myids.size() < 1) {
    throw http::bad_request("The parameter ways is required, and must be "
                            "of the form ways=id[,id[,id...]].");
  }

  return myids;
}

} // namespace api06
