#include "cgimap/api06/changeset_download_handler.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/config.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

changeset_download_responder::changeset_download_responder(
  mime::type mt, osm_changeset_id_t id_, data_selection_ptr &w_)
  : osmchange_responder(mt, w_), id(id_) {

  if (sel->select_changesets({id}) == 0) {
    std::ostringstream error;
    error << "Changeset " << id << " was not found.";
    throw http::not_found(error.str());
  }
  sel->select_historical_by_changesets({id});
}

changeset_download_responder::~changeset_download_responder() = default;

changeset_download_handler::changeset_download_handler(request &req, osm_changeset_id_t id_)
  : id(id_) {
}

changeset_download_handler::~changeset_download_handler() = default;

std::string changeset_download_handler::log_name() const { return "changeset/download"; }

responder_ptr_t changeset_download_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new changeset_download_responder(mime_type, id, w));
}

} // namespace api06
