#include "cgimap/api06/node_history_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

node_history_responder::node_history_responder(mime::type mt, osm_nwr_id_t id_, data_selection_ptr &w_)
  : osm_current_responder(mt, w_), id(id_) {
  vector<osm_nwr_id_t> ids;
  ids.push_back(id);

  if (sel->select_nodes_with_history(ids) == 0) {
    throw http::not_found("");
  }
}

node_history_responder::~node_history_responder() {}

node_history_handler::node_history_handler(request &, osm_nwr_id_t id_) : id(id_) {}

node_history_handler::~node_history_handler() {}

std::string node_history_handler::log_name() const { return "node/history"; }

responder_ptr_t node_history_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new node_history_responder(mime_type, id, w));
}

} // namespace api06
