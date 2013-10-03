#include "api06/node_ways_handler.hpp"
#include "http.hpp"

#include <sstream>

using std::stringstream;
using std::list;

namespace api06 {

node_ways_responder::node_ways_responder(mime::type mt, osm_id_t id_, data_selection &w_)
  : osm_responder(mt, w_), id(id_) {
  list<osm_id_t> ids;
  ids.push_back(id);

  if (sel.select_nodes(ids) == 0) {
    throw http::not_found("");
  }
  else {
    sel.select_ways_from_nodes();
    check_visibility();
  }
}

node_ways_responder::~node_ways_responder() {
}

node_ways_handler::node_ways_handler(FCGX_Request &request, osm_id_t id_) 
	: id(id_) {
}

node_ways_handler::~node_ways_handler() {
}

std::string 
node_ways_handler::log_name() const {
	return "node/ways";
}

responder_ptr_t 
node_ways_handler::responder(data_selection &x) const {
	return responder_ptr_t(new node_ways_responder(mime_type, id, x));
}

void
node_ways_responder::check_visibility() {
  if (sel.check_node_visibility(id) == data_selection::deleted) {
    throw http::gone(); // TODO: fix error message / throw structure to emit better error message
  }
}

} // namespace api06

