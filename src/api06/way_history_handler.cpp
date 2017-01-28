#include "cgimap/api06/way_history_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

way_history_responder::way_history_responder(mime::type mt, osm_nwr_id_t id_, data_selection_ptr &w_)
  : osm_current_responder(mt, w_), id(id_) {
  vector<osm_nwr_id_t> ids;
  ids.push_back(id);

  if (sel->select_ways_with_history(ids) == 0) {
    throw http::not_found("");
  }
}

way_history_responder::~way_history_responder() {}

way_history_handler::way_history_handler(request &, osm_nwr_id_t id_) : id(id_) {}

way_history_handler::~way_history_handler() {}

std::string way_history_handler::log_name() const { return "way/history"; }

responder_ptr_t way_history_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new way_history_responder(mime_type, id, w));
}

} // namespace api06
