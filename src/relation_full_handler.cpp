#include "relation_full_handler.hpp"
#include "osm_helpers.hpp"
#include "http.hpp"
#include "logger.hpp"
#include <sstream>
#include <boost/format.hpp>

using std::stringstream;

relation_full_responder::relation_full_responder(mime::type mt_, id_t id_, pqxx::work &w_) 
  : osm_responder(mt_, w_), id(id_) {
  check_visibility();
  
  stringstream query;
  query << "create temporary table tmp_relations as select id from current_relations where id = " << id << " and visible";
  w.exec(query);

  osm_helpers::create_tmp_nodes_from_relations(w);
  osm_helpers::create_tmp_ways_from_relations(w);
  osm_helpers::insert_tmp_nodes_from_way_nodes(w);
  osm_helpers::insert_tmp_relations_from_relations(w);  
}

relation_full_responder::~relation_full_responder() throw() {
}

void
relation_full_responder::check_visibility() {
  stringstream query;
  query << "select visible from relations where id = " << id;
  pqxx::result res = w.exec(query);
  if (res.size() == 0) {
    throw http::not_found(""); // TODO: fix error message / throw structure to emit better error message
  }
  if (!res[0][0].as<bool>()) {
    throw http::gone(); // TODO: fix error message / throw structure to emit better error message
  }  
}

relation_full_handler::relation_full_handler(FCGX_Request &request, id_t id_)
  : id(id_) {
  logger::message((boost::format("starting relation/full handler with id = %1%") % id).str());
}

relation_full_handler::~relation_full_handler() throw() {
}

std::string 
relation_full_handler::log_name() const {
  return "relation/full";
}

responder_ptr_t 
relation_full_handler::responder(pqxx::work &x) const {
  return responder_ptr_t(new relation_full_responder(mime_type, id, x));
}

