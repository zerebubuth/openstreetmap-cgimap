#include "api06/relation_handler.hpp"
#include "http.hpp"

#include <sstream>

using std::stringstream;
using std::list;

namespace api06 {

relation_responder::relation_responder(mime::type mt, osm_id_t id_, data_selection &w_)
  : osm_responder(mt, w_), id(id_) {
  list<osm_id_t> ids;
  ids.push_back(id);

  if (sel.select_relations(ids) == 0) {
    throw http::not_found("");
  }
  else {
    check_visibility();
  }
}

relation_responder::~relation_responder() {
}

relation_handler::relation_handler(FCGX_Request &request, osm_id_t id_) 
	: id(id_) {
}

relation_handler::~relation_handler() {
}

std::string 
relation_handler::log_name() const {
	return "relation";
}

responder_ptr_t 
relation_handler::responder(data_selection &x) const {
	return responder_ptr_t(new relation_responder(mime_type, id, x));
}

void
relation_responder::check_visibility() {
  if (sel.check_relation_visibility(id) == data_selection::deleted) {
    throw http::gone(); // TODO: fix error message / throw structure to emit better error message
  }
}

} // namespace api06
