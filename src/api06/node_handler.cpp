#include "cgimap/api06/node_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>
#include <fmt/core.h>

using std::vector;

namespace api06 {

node_responder::node_responder(mime::type mt, osm_nwr_id_t id_, data_selection &w_)
    : osm_current_responder(mt, w_), id(id_) {

  if (sel.select_nodes({id}) == 0) {
    throw http::not_found(fmt::format("Node {:d} was not found.", id));
  }
  check_visibility();
}

node_handler::node_handler(request &, osm_nwr_id_t id_) : id(id_) {}

std::string node_handler::log_name() const { return "node"; }

responder_ptr_t node_handler::responder(data_selection &w) const {
  return responder_ptr_t(new node_responder(mime_type, id, w));
}

void node_responder::check_visibility() {
  if (sel.check_node_visibility(id) == data_selection::deleted) {
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();
  }
}

} // namespace api06
