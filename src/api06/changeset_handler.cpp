#include "cgimap/api06/changeset_handler.hpp"
#include "cgimap/http.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

changeset_responder::changeset_responder(mime::type mt, osm_changeset_id_t id_, factory_ptr &w_)
    : osm_current_responder(mt, w_), id(id_) {
  vector<osm_changeset_id_t> ids;
  ids.push_back(id);

  if (!sel->supports_changesets()) {
    throw http::server_error("Data source does not support changesets.");
  }

  if (sel->select_changesets(ids) == 0) {
    throw http::not_found("");
  }
}

changeset_responder::~changeset_responder() {}

changeset_handler::changeset_handler(request &, osm_changeset_id_t id_) : id(id_) {}

changeset_handler::~changeset_handler() {}

std::string changeset_handler::log_name() const { return "changeset"; }

responder_ptr_t changeset_handler::responder(factory_ptr &w) const {
  return responder_ptr_t(new changeset_responder(mime_type, id, w));
}

} // namespace api06
