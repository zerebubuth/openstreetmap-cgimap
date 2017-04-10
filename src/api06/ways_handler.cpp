#include "cgimap/api06/ways_handler.hpp"
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

ways_responder::ways_responder(mime::type mt, vector<id_version> ids_,
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

  size_t num_selected = sel->select_ways(current_ids);
  if (!historic_ids.empty()) {
    if (sel->supports_historical_versions()) {
      num_selected += sel->select_historical_ways(historic_ids);
    } else {
      throw http::server_error("Data source does not support historical versions.");
    }
  }

  if (num_selected != ids.size()) {
    throw http::not_found("One or more of the ways were not found.");
  }
}

ways_responder::~ways_responder() {}

ways_handler::ways_handler(request &req) : ids(validate_request(req)) {}

ways_handler::~ways_handler() {}

std::string ways_handler::log_name() const {
  stringstream msg;
  msg << "ways?ways=";
  std::copy(ids.begin(), ids.end(),
            infix_ostream_iterator<id_version>(msg, ", "));
  return msg.str();
}

responder_ptr_t ways_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new ways_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or
 * throwing an error if there was no valid list of way ids.
 */
vector<id_version> ways_handler::validate_request(request &req) {
  vector<id_version> myids = parse_id_list_params(req, "ways");

  if (myids.size() < 1) {
    throw http::bad_request(
      "The parameter ways is required, and must be "
      "of the form ways=ID[vVER][,ID[vVER][,ID[vVER]...]].");
  }

  return myids;
}

} // namespace api06
