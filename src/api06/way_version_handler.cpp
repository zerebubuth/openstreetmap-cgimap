#include "cgimap/api06/way_version_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

way_version_responder::way_version_responder(mime::type mt, osm_nwr_id_t id_, osm_version_t v_, data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_), v(v_) {

  if (sel->select_historical_ways({std::make_pair(id, v)}) == 0) {
     throw http::not_found("");
  }
}

way_version_responder::~way_version_responder() = default;

way_version_handler::way_version_handler(request &, osm_nwr_id_t id_, osm_version_t v_) : id(id_), v(v_) {}

way_version_handler::~way_version_handler() = default;

std::string way_version_handler::log_name() const { return "way"; }

responder_ptr_t way_version_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new way_version_responder(mime_type, id, v, x));
}

} // namespace api06
