#include "cgimap/api06/changeset_upload_handler.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/config.hpp"

#include <sstream>

using std::stringstream;
using std::vector;

namespace api06 {

changeset_upload_responder::changeset_upload_responder(
  mime::type mt, osm_changeset_id_t id_, data_selection_ptr &w_)
  : osmchange_responder(mt, w_), id(id_) {

  throw http::server_error("unimplemented");
}

changeset_upload_responder::~changeset_upload_responder() {}

changeset_upload_handler::changeset_upload_handler(request &req, osm_changeset_id_t id_)
  : handler(mime::unspecified_type, http::method::POST | http::method::OPTIONS)
  , id(id_) {
}

changeset_upload_handler::~changeset_upload_handler() {}

std::string changeset_upload_handler::log_name() const { return "changeset/download"; }

responder_ptr_t changeset_upload_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new changeset_upload_responder(mime_type, id, w));
}

} // namespace api06
