#include "cgimap/api06/changeset_handler.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/config.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

changeset_responder::changeset_responder(mime::type mt, osm_changeset_id_t id_,
                                         bool include_discussion_,
                                         data_selection_ptr &w_)
  : osm_current_responder(mt, w_), id(id_),
    include_discussion(include_discussion_) {
  vector<osm_changeset_id_t> ids;
  ids.push_back(id);

  if (!sel->supports_changesets()) {
    throw http::server_error("Data source does not support changesets.");
  }

  if (sel->select_changesets(ids) == 0) {
    std::ostringstream error;
    error << "Changeset " << id << " was not found.";
    throw http::not_found(error.str());
  }

  if (include_discussion) {
    sel->select_changeset_discussions();
  }
}

changeset_responder::~changeset_responder() {}

namespace {
// functor to use in find_if to locate the "include_discussion" header
struct discussion_header_finder {
  bool operator()(const std::pair<std::string, std::string> &header) const {
    static const std::string target("include_discussion");
    return header.first == target;
  }
};
}

changeset_handler::changeset_handler(request &req, osm_changeset_id_t id_)
  : id(id_), include_discussion(false) {
  using std::vector;
  using std::pair;
  using std::string;

  string decoded = http::urldecode(get_query_string(req));
  const vector<pair<string, string> > params = http::parse_params(decoded);
  vector<pair<string, string> >::const_iterator itr =
    std::find_if(params.begin(), params.end(), discussion_header_finder());

  include_discussion = (itr != params.end());
}

changeset_handler::~changeset_handler() {}

std::string changeset_handler::log_name() const { return "changeset"; }

responder_ptr_t changeset_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new changeset_responder(mime_type, id, include_discussion, w));
}

} // namespace api06
