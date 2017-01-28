#include "cgimap/api06/nodes_handler.hpp"
#include "cgimap/api06/handler_utils.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/infix_ostream_iterator.hpp"
#include "cgimap/api06/id_version_io.hpp"

#include <boost/foreach.hpp>

#include <sstream>

using std::stringstream;
using std::vector;
using std::string;

namespace api06 {

nodes_responder::nodes_responder(mime::type mt, vector<id_version> ids_,
                                 data_selection_ptr &w_)
    : osm_current_responder(mt, w_), ids(ids_) {

  vector<osm_nwr_id_t> current_ids;
  vector<osm_edition_t> historic_ids;

  BOOST_FOREACH(id_version idv, ids_) {
    if (idv.version) {
      historic_ids.push_back(std::make_pair(idv.id, *idv.version));
    } else {
      current_ids.push_back(idv.id);
    }
  }

  size_t num_selected = sel->select_nodes(current_ids);
  if (!historic_ids.empty()) {
    if (sel->supports_historical_versions()) {
      num_selected += sel->select_historical_nodes(historic_ids);
    } else {
      throw http::server_error("Data source does not support historical versions.");
    }
  }

  if (num_selected != ids.size()) {
    throw http::not_found("One or more of the nodes were not found.");
  }
}

nodes_responder::~nodes_responder() {}

nodes_handler::nodes_handler(request &req) : ids(validate_request(req)) {}

nodes_handler::~nodes_handler() {}

std::string nodes_handler::log_name() const {
  stringstream msg;
  msg << "nodes?nodes=";
  std::copy(ids.begin(), ids.end(),
            infix_ostream_iterator<id_version>(msg, ", "));
  return msg.str();
}

responder_ptr_t nodes_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new nodes_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or
 * throwing an error if there was no valid list of node ids.
 */
vector<id_version> nodes_handler::validate_request(request &req) {
  vector<id_version> ids = parse_id_list_params(req, "nodes");

  if (ids.size() < 1) {
    throw http::bad_request(
      "The parameter nodes is required, and must be "
      "of the form nodes=ID[vVER][,ID[vVER][,ID[vVER]...]].");
  }

  return ids;
}

} // namespace api06
