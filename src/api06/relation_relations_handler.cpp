#include "cgimap/api06/relation_relations_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

relation_relations_responder::relation_relations_responder(mime::type mt, osm_nwr_id_t id_,
                                         data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {
  vector<osm_nwr_id_t> ids;
  ids.push_back(id);

  if (sel->select_relations(ids) == 0) {
    std::ostringstream error;
    error << "Relation " << id << " was not found.";
    throw http::not_found(error.str());
  } else {
    sel->select_relations_from_relations();
  }
}

relation_relations_responder::~relation_relations_responder() {}

relation_relations_handler::relation_relations_handler(request &, osm_nwr_id_t id_) : id(id_) {}

relation_relations_handler::~relation_relations_handler() {}

std::string relation_relations_handler::log_name() const { return "relation/relations"; }

responder_ptr_t relation_relations_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new relation_relations_responder(mime_type, id, x));
}

} // namespace api06
