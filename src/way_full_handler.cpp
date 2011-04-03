#include "way_full_handler.hpp"
#include "osm_helpers.hpp"
#include "http.hpp"
#include "logger.hpp"
#include <sstream>
#include <boost/format.hpp>

using std::stringstream;

way_full_responder::way_full_responder(mime::type mt_, id_t id_, pqxx::work &w_) 
	: osm_responder(mt_, w_, true, true, false), id(id_) {
  check_visibility();
  
  stringstream query;
  query << "create temporary table tmp_ways as select id from current_ways where id = " << id << " and visible";
  w.exec(query);

  osm_helpers::create_tmp_nodes_from_way_nodes(w);
}

way_full_responder::~way_full_responder() throw() {
}

void
way_full_responder::check_visibility() {
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

way_full_handler::way_full_handler(FCGX_Request &request, id_t id_)
  : id(id_) {
  logger::message((boost::format("starting way/full handler with id = %1%") % id).str());
}

way_full_handler::~way_full_handler() throw() {
}

std::string 
way_full_handler::log_name() const {
  return "way/full";
}

responder_ptr_t 
way_full_handler::responder(pqxx::work &x) const {
  return responder_ptr_t(new way_full_responder(mime_type, id, x));
}

