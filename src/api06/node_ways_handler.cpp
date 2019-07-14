#include "cgimap/api06/node_ways_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

node_ways_responder::node_ways_responder(mime::type mt, osm_nwr_id_t id_,
                                         data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel->select_nodes({ id }) > 0 && is_visible()) {
    sel->select_ways_from_nodes();
  }
  sel->drop_nodes();
}

node_ways_responder::~node_ways_responder() = default;

node_ways_handler::node_ways_handler(request &, osm_nwr_id_t id_) : id(id_) {}

node_ways_handler::~node_ways_handler() = default;

std::string node_ways_handler::log_name() const { return "node/ways"; }

responder_ptr_t node_ways_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new node_ways_responder(mime_type, id, x));
}

bool node_ways_responder::is_visible() {
  return (!sel->check_node_visibility(id) == data_selection::deleted);
}

} // namespace api06
