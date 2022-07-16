#include "cgimap/api06/way_full_handler.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include <sstream>
#include <fmt/core.h>

using std::vector;

namespace api06 {

way_full_responder::way_full_responder(mime::type mt_, osm_nwr_id_t id_,
                                       data_selection &w_)
    : osm_current_responder(mt_, w_), id(id_) {

  if (sel.select_ways({id}) == 0) {
    throw http::not_found(fmt::format("Way {:d} was not found.", id));
  }
  check_visibility();
  sel.select_nodes_from_way_nodes();
}

void way_full_responder::check_visibility() {
  switch (sel.check_way_visibility(id)) {

  case data_selection::non_exist:
    throw http::not_found(fmt::format("Way {:d} was not found.", id));

  case data_selection::deleted:
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();

  default:
    break;
  }
}

way_full_handler::way_full_handler(request &, osm_nwr_id_t id_) : id(id_) {
  logger::message(
      fmt::format("starting way/full handler with id = {:d}", id));
}

std::string way_full_handler::log_name() const { return "way/full"; }

responder_ptr_t way_full_handler::responder(data_selection &x) const {
  return responder_ptr_t(new way_full_responder(mime_type, id, x));
}

} // namespace api06
