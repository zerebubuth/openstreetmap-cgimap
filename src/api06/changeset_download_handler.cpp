#include "cgimap/api06/changeset_download_handler.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/config.hpp"

#include <sstream>
#include <fmt/core.h>

using std::stringstream;
using std::vector;

namespace api06 {

changeset_download_responder::changeset_download_responder(
  mime::type mt, osm_changeset_id_t id_, data_selection &w_)
  : osmchange_responder(mt, w_), id(id_) {

  if (sel.select_changesets({id}) == 0) {
    throw http::not_found(fmt::format("Changeset {:d} was not found.", id));
  }
  sel.select_historical_by_changesets({id});
}

changeset_download_handler::changeset_download_handler(request &req, osm_changeset_id_t id_)
  : id(id_) {
}

std::string changeset_download_handler::log_name() const { return "changeset/download"; }

responder_ptr_t changeset_download_handler::responder(data_selection &w) const {
  return responder_ptr_t(new changeset_download_responder(mime_type, id, w));
}

} // namespace api06
