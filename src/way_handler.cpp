#include "way_handler.hpp"
#include "http.hpp"

#include <sstream>

using std::stringstream;
using std::list;

way_responder::way_responder(mime::type mt, id_t id_, data_selection &w_)
	: osm_responder(mt, w_), id(id_) {
	list<id_t> ids;

	check_visibility();

	sel.select_visible_ways(ids);
	sel.select_nodes_from_way_nodes();
}

way_responder::~way_responder() {
}

way_handler::way_handler(FCGX_Request &request, id_t id_) 
	: id(id_) {
}

way_handler::~way_handler() {
}

std::string 
way_handler::log_name() const {
	return "way";
}

responder_ptr_t 
way_handler::responder(data_selection &x) const {
	return responder_ptr_t(new way_responder(mime_type, id, x));
}

void
way_responder::check_visibility() {
	switch (sel.check_way_visibility(id)) {

	case data_selection::non_exist:
		throw http::not_found(""); // TODO: fix error message / throw structure to emit better error message
	
	case data_selection::deleted:
		throw http::gone(); // TODO: fix error message / throw structure to emit better error message
	}
}
