#include "cgimap/api06/relation_version_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

relation_version_responder::relation_version_responder(mime::type mt, osm_nwr_id_t id_, 
                                       osm_version_t v_, data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_), v(v_) {
  vector<osm_edition_t> historic_ids;
  historic_ids.push_back(std::make_pair(id, v));
  if (sel->supports_historical_versions()) {
    if (sel->select_historical_relations(historic_ids) == 0) {
       throw http::not_found("");
    }
  } else {
   throw http::server_error("Data source does not support historical versions.");
  }
}

relation_version_responder::~relation_version_responder() {}

relation_version_handler::relation_version_handler(request &, osm_nwr_id_t id_, osm_version_t v_) : id(id_), v(v_) {}

relation_version_handler::~relation_version_handler() {}

std::string relation_version_handler::log_name() const { return "relation"; }

responder_ptr_t relation_version_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new relation_version_responder(mime_type, id, v, x));
}

} // namespace api06
