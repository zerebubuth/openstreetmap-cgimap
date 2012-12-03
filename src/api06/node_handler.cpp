#include "api06/node_handler.hpp"
#include "http.hpp"

#include <sstream>

using std::stringstream;
using std::list;

namespace api06 {

node_responder::node_responder(mime::type mt, id_t id_, data_selection &w_)
	: osm_responder(mt, w_), id(id_) {
	list<id_t> ids;

	check_visibility();

	ids.push_back(id);
	sel.select_nodes(ids);
}

node_responder::~node_responder() {
}

node_handler::node_handler(FCGX_Request &request, id_t id_) 
	: id(id_) {
}

node_handler::~node_handler() {
}

std::string 
node_handler::log_name() const {
	return "node";
}

responder_ptr_t 
node_handler::responder(data_selection &x) const {
	return responder_ptr_t(new node_responder(mime_type, id, x));
}

void
node_responder::check_visibility() {
	switch (sel.check_node_visibility(id)) {

	case data_selection::non_exist:
		throw http::not_found(""); // TODO: fix error message / throw structure to emit better error message
	
	case data_selection::deleted:
		throw http::gone(); // TODO: fix error message / throw structure to emit better error message
	}
}

} // namespace api06

