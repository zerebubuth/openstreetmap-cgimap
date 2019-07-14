#include "cgimap/api06/relation_relations_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

relation_relations_responder::relation_relations_responder(mime::type mt, osm_nwr_id_t id_,
                                         data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel->select_relations({id}) > 0 && is_visible()) {
    sel->select_relations_from_relations(true);
  }
  else
    sel->drop_relations();
}

relation_relations_responder::~relation_relations_responder() {}

relation_relations_handler::relation_relations_handler(request &, osm_nwr_id_t id_) : id(id_) {}

relation_relations_handler::~relation_relations_handler() {}

std::string relation_relations_handler::log_name() const { return "relation/relations"; }

responder_ptr_t relation_relations_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new relation_relations_responder(mime_type, id, x));
}

bool relation_relations_responder::is_visible() {
  return (!sel->check_relation_visibility(id) == data_selection::deleted);
}

} // namespace api06
