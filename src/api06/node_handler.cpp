#include "cgimap/api06/node_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

node_responder::node_responder(mime::type mt, osm_nwr_id_t id_, data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {
  vector<osm_nwr_id_t> ids;
  ids.push_back(id);

  if (sel->select_nodes(ids) == 0) {
    std::ostringstream error;
    error << "Node " << id << " was not found.";
    throw http::not_found(error.str());
  } else {
    check_visibility();
  }
}

node_responder::~node_responder() {}

node_handler::node_handler(request &, osm_nwr_id_t id_) : id(id_) {}

node_handler::~node_handler() {}

std::string node_handler::log_name() const { return "node"; }

responder_ptr_t node_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new node_responder(mime_type, id, w));
}

void node_responder::check_visibility() {
  if (sel->check_node_visibility(id) == data_selection::deleted) {
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();
  }
}

} // namespace api06
