#include "cgimap/api06/relations_handler.hpp"
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

relations_responder::relations_responder(mime::type mt, vector<id_version> ids_,
                                         data_selection_ptr &s_)
    : osm_current_responder(mt, s_), ids(ids_) {
  vector<osm_nwr_id_t> current_ids;
  vector<osm_edition_t> historic_ids;

  BOOST_FOREACH(id_version idv, ids_) {
    if (idv.version) {
      historic_ids.push_back(std::make_pair(idv.id, *idv.version));
    } else {
      current_ids.push_back(idv.id);
    }
  }

  size_t num_selected = sel->select_relations(current_ids);
  if (!historic_ids.empty()) {
    if (sel->supports_historical_versions()) {
      num_selected += sel->select_historical_relations(historic_ids);
    } else {
      throw http::server_error("Data source does not support historical versions.");
    }
  }

  if (num_selected != ids.size()) {
    throw http::not_found("One or more of the relations were not found.");
  }
}

relations_responder::~relations_responder() {}

relations_handler::relations_handler(request &req)
    : ids(validate_request(req)) {}

relations_handler::~relations_handler() {}

std::string relations_handler::log_name() const {
  stringstream msg;
  msg << "relations?relations=";
  std::copy(ids.begin(), ids.end(),
            infix_ostream_iterator<id_version>(msg, ", "));
  return msg.str();
}

responder_ptr_t relations_handler::responder(data_selection_ptr &x) const {
  return responder_ptr_t(new relations_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or
 * throwing an error if there was no valid list of node ids.
 */
vector<id_version> relations_handler::validate_request(request &req) {
  vector<id_version> myids = parse_id_list_params(req, "relations");

  if (myids.size() < 1) {
    throw http::bad_request("The parameter relations is required, and must be "
                            "of the form relations=id[,id[,id...]].");
  }

  return myids;
}

} // namespace api06
