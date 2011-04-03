#include "way_handler.hpp"
#include "osm_helpers.hpp"
#include "http.hpp"

#include <sstream>

using std::stringstream;

way_responder::way_responder(mime::type mt, id_t id_, pqxx::work &w_)
	: osm_responder(mt, w_), id(id_) {
	check_visibility();

	stringstream query;
	query << "create temporary table tmp_ways as select id from current_ways where id = " << id << " and visible";
	w.exec(query);

	osm_helpers::insert_tmp_nodes_from_way_nodes(w);
}

way_responder::~way_responder() throw() {
}

way_handler::way_handler(FCGX_Request &request, id_t id_) 
	: id(id_) {
}

way_handler::~way_handler() throw() {
}

std::string 
way_handler::log_name() const {
	return "way";
}

responder_ptr_t 
way_handler::responder(pqxx::work &x) const {
	return responder_ptr_t(new way_responder(mime_type, id, x));
}

void
way_responder::check_visibility() {
	stringstream query;
	query << "select visible from ways where id = " << id;
	pqxx::result res = w.exec(query);
	if (res.size() == 0) {
		throw http::not_found(""); // TODO: fix error message / throw structure to emit better error message
	}
	if (!res[0][0].as<bool>()) {
		throw http::gone(); // TODO: fix error message / throw structure to emit better error message
	}
}
