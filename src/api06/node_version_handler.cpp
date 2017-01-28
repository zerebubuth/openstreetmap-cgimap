#include "cgimap/api06/node_version_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

node_version_responder::node_version_responder(mime::type mt, osm_nwr_id_t id_, osm_version_t v_, data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_), v(v_) {
  vector<osm_edition_t> historic_ids;
  historic_ids.push_back(std::make_pair(id, v));
  if (sel->supports_historical_versions()) {
    if (sel->select_historical_nodes(historic_ids) == 0) {
       throw http::not_found("");
    }
  } else {
   throw http::server_error("Data source does not support historical versions.");
  }
}

node_version_responder::~node_version_responder() {}

node_version_handler::node_version_handler(request &, osm_nwr_id_t id_, osm_version_t v_) : id(id_), v(v_) {}

node_version_handler::~node_version_handler() {}

std::string node_version_handler::log_name() const { return "node"; }

responder_ptr_t node_version_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new node_version_responder(mime_type, id, v, w));
}

} // namespace api06
