#include "cgimap/api06/way_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

way_responder::way_responder(mime::type mt, osm_nwr_id_t id_, data_selection_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {
  vector<osm_nwr_id_t> ids;
  ids.push_back(id);

  if (sel->select_ways(ids) == 0) {
    std::ostringstream error;
    error << "Way " << id << " was not found.";
    throw http::not_found(error.str());
  } else {
    check_visibility();
  }
}

way_responder::~way_responder() {}

way_handler::way_handler(request &, osm_nwr_id_t id_) : id(id_) {}

way_handler::~way_handler() {}

std::string way_handler::log_name() const { return "way"; }

responder_ptr_t way_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new way_responder(mime_type, id, x));
}

void way_responder::check_visibility() {
  if (sel->check_way_visibility(id) == data_selection::deleted) {
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();
  }
}

} // namespace api06
