#include "cgimap/api06/way_full_handler.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include <sstream>
#include <boost/format.hpp>

using std::stringstream;
using std::list;

namespace api06 {

way_full_responder::way_full_responder(mime::type mt_, osm_id_t id_,
                                       factory_ptr &w_)
    : osm_current_responder(mt_, w_), id(id_) {
  list<osm_id_t> ids;
  ids.push_back(id);

  if (sel->select_ways(ids) == 0) {
    throw http::not_found("");
  } else {
    check_visibility();
  }

  sel->select_nodes_from_way_nodes();
}

way_full_responder::~way_full_responder() {}

void way_full_responder::check_visibility() {
  switch (sel->check_way_visibility(id)) {

  case data_selection::non_exist:
    // TODO: fix error message / throw structure to emit better error message
    throw http::not_found("");

  case data_selection::deleted:
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();

  default:
    break;
  }
}

way_full_handler::way_full_handler(request &, osm_id_t id_) : id(id_) {
  logger::message(
      (boost::format("starting way/full handler with id = %1%") % id).str());
}

way_full_handler::~way_full_handler() {}

std::string way_full_handler::log_name() const { return "way/full"; }

responder_ptr_t way_full_handler::responder(factory_ptr &x) const {
  return responder_ptr_t(new way_full_responder(mime_type, id, x));
}

} // namespace api06
