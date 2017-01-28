#include "cgimap/api06/relation_history_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

relation_history_responder::relation_history_responder(mime::type mt, osm_nwr_id_t id_, data_selection_ptr &w_)
  : osm_current_responder(mt, w_), id(id_) {
  vector<osm_nwr_id_t> ids;
  ids.push_back(id);

  if (sel->select_relations_with_history(ids) == 0) {
    throw http::not_found("");
  }
}

relation_history_responder::~relation_history_responder() {}

relation_history_handler::relation_history_handler(request &, osm_nwr_id_t id_) : id(id_) {}

relation_history_handler::~relation_history_handler() {}

std::string relation_history_handler::log_name() const { return "relation/history"; }

responder_ptr_t relation_history_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new relation_history_responder(mime_type, id, w));
}

} // namespace api06
