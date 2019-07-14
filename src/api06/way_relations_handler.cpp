#include "cgimap/api06/way_relations_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

way_relations_responder::way_relations_responder(mime::type mt, osm_nwr_id_t id_,
                                         data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel->select_ways({id}) > 0 && is_visible()) {
    sel->select_relations_from_ways();
  }
  sel->drop_ways();
}

way_relations_responder::~way_relations_responder() {}

way_relations_handler::way_relations_handler(request &, osm_nwr_id_t id_) : id(id_) {}

way_relations_handler::~way_relations_handler() {}

std::string way_relations_handler::log_name() const { return "way/relations"; }

responder_ptr_t way_relations_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new way_relations_responder(mime_type, id, x));
}

bool way_relations_responder::is_visible() {
  return (!sel->check_way_visibility(id) == data_selection::deleted);
}

} // namespace api06
