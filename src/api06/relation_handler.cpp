#include "cgimap/api06/relation_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

relation_responder::relation_responder(mime::type mt, osm_nwr_id_t id_,
                                       data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {
  vector<osm_nwr_id_t> ids;
  ids.push_back(id);

  if (sel->select_relations(ids) == 0) {
    std::ostringstream error;
    error << "Relation " << id << " was not found.";
    throw http::not_found(error.str());
  } else {
    check_visibility();
  }
}

relation_responder::~relation_responder() {}

relation_handler::relation_handler(request &, osm_nwr_id_t id_) : id(id_) {}

relation_handler::~relation_handler() {}

std::string relation_handler::log_name() const { return "relation"; }

responder_ptr_t relation_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new relation_responder(mime_type, id, x));
}

void relation_responder::check_visibility() {
  if (sel->check_relation_visibility(id) == data_selection::deleted) {
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();
  }
}

} // namespace api06
