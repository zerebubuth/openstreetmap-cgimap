#include "node_handler.hpp"
#include "osm_helpers.hpp"
#include "http.hpp"

#include <sstream>

using std::stringstream;

node_responder::node_responder(id_t id_, pqxx::work &w_)
	: id(id_), w(w_) {
	check_visibility();

	stringstream query;
	query << "create temporary table tmp_nodes as select id from current_nodes where id = " << id << " and visible";
	w.exec(query);
}

node_responder::~node_responder() throw() {
}

void 
node_responder::write(std::auto_ptr<output_formatter> f) {
	try {
		f->start_document();
		osm_helpers::write_tmp_nodes(w, *f, 1);
	} catch (const std::exception &e) {
		f->error(e);
	}
	f->end_document();
}

node_handler::node_handler(FCGX_Request &request, id_t id_) 
	: id(id_) {
}

node_handler::~node_handler() throw() {
}

std::string 
node_handler::log_name() const {
	return "node";
}

responder_ptr_t 
node_handler::responder(pqxx::work &x) const {
	return responder_ptr_t(new node_responder(id, x));
}

formats::format_type 
node_handler::format() const {
	return formats::XML;
}

void
node_responder::check_visibility() {
	stringstream query;
	query << "select visible from nodes where id = " << id;
	pqxx::result res = w.exec(query);
	if (res.size() == 0) {
		throw http::not_found(""); // TODO: fix error message / throw structure to emit better error message
	}
	if (!res[0][0].as<bool>()) {
		throw http::gone(); // TODO: fix error message / throw structure to emit better error message
	}
}
