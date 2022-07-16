#include "cgimap/api06/relation_full_handler.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include <sstream>
#include <fmt/core.h>

using std::vector;

namespace api06 {

relation_full_responder::relation_full_responder(mime::type mt_, osm_nwr_id_t id_,
                                                 data_selection &w_)
    : osm_current_responder(mt_, w_), id(id_) {

  if (sel.select_relations({id}) == 0) {
    throw http::not_found(fmt::format("Relation {:d} was not found.", id));
  }

  check_visibility();
  sel.select_nodes_from_relations();
  sel.select_ways_from_relations();
  sel.select_nodes_from_way_nodes();
  sel.select_relations_members_of_relations();
}

void relation_full_responder::check_visibility() {
  switch (sel.check_relation_visibility(id)) {

  case data_selection::non_exist:
    throw http::not_found(fmt::format("Relation {:d} was not found.", id));

  case data_selection::deleted:
    // TODO: fix error message / throw structure to emit better error message
    throw http::gone();

  default:
    break;
  }
}

relation_full_handler::relation_full_handler(request &, osm_nwr_id_t id_)
    : id(id_) {
  logger::message(
      fmt::format("starting relation/full handler with id = {:d}", id));
}

std::string relation_full_handler::log_name() const { return "relation/full"; }

responder_ptr_t relation_full_handler::responder(data_selection &x) const {
  return responder_ptr_t(new relation_full_responder(mime_type, id, x));
}

} // namespace api06
