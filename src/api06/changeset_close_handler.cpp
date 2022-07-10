#include "cgimap/config.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"

#include "cgimap/api06/changeset_close_handler.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <sstream>


namespace api06 {

changeset_close_responder::changeset_close_responder(
    mime::type mt, data_update & upd, osm_changeset_id_t id_, const std::string &payload,
    std::optional<osm_user_id_t> user_id)
    : text_responder(mt) {

  osm_changeset_id_t changeset = id_;
  osm_user_id_t uid = *user_id;

  auto changeset_updater = upd.get_changeset_updater(changeset, uid);

  changeset_updater->api_close_changeset();

  upd.commit();
}


changeset_close_handler::changeset_close_handler(request &,
                                                   osm_changeset_id_t id_)
    : payload_enabled_handler(mime::text_plain,
                              http::method::PUT | http::method::OPTIONS),
      id(id_) {}


std::string changeset_close_handler::log_name() const {
  return (fmt::format("changeset/close {:d}", id));
}

responder_ptr_t
changeset_close_handler::responder(data_selection &) const {
  throw http::server_error(
      "changeset_close_handler: data_selection unsupported");
}

responder_ptr_t changeset_close_handler::responder(
    data_update & upd, const std::string &payload, std::optional<osm_user_id_t> user_id) const {
  return responder_ptr_t(
      new changeset_close_responder(mime_type, upd, id, payload, user_id));
}

bool changeset_close_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
