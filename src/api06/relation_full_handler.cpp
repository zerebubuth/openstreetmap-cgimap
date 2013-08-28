#include "api06/relation_full_handler.hpp"
#include "http.hpp"
#include "logger.hpp"
#include <sstream>
#include <boost/format.hpp>

using std::stringstream;
using std::list;

namespace api06 {

relation_full_responder::relation_full_responder(mime::type mt_, osm_id_t id_, data_selection &w_) 
  : osm_responder(mt_, w_), id(id_) {
  list<osm_id_t> ids;
  ids.push_back(id);

  if (sel.select_relations(ids) == 0) {
    throw http::not_found("");
  } else {
    check_visibility();
  }

  sel.select_nodes_from_relations();
  sel.select_ways_from_relations();
  sel.select_nodes_from_way_nodes();
  sel.select_relations_members_of_relations();
}

relation_full_responder::~relation_full_responder() {
}

void
relation_full_responder::check_visibility() {
	switch (sel.check_relation_visibility(id)) {

	case data_selection::non_exist:
		throw http::not_found(""); // TODO: fix error message / throw structure to emit better error message
	
	case data_selection::deleted:
		throw http::gone(); // TODO: fix error message / throw structure to emit better error message
	}
}

relation_full_handler::relation_full_handler(FCGX_Request &request, osm_id_t id_)
  : id(id_) {
  logger::message((boost::format("starting relation/full handler with id = %1%") % id).str());
}

relation_full_handler::~relation_full_handler() {
}

std::string 
relation_full_handler::log_name() const {
  return "relation/full";
}

responder_ptr_t 
relation_full_handler::responder(data_selection &x) const {
  return responder_ptr_t(new relation_full_responder(mime_type, id, x));
}

} // namespace api06

