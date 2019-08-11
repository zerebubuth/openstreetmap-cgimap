#include "cgimap/config.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"

#include "cgimap/api06/changeset_create_handler.hpp"
#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <sstream>


namespace api06 {

changeset_create_responder::changeset_create_responder(
    mime::type mt, data_update_ptr & upd, const std::string &payload,
    boost::optional<osm_user_id_t> user_id)
    : text_responder(mt) {

  osm_changeset_id_t changeset = 0;

  osm_user_id_t uid = *user_id;

  auto changeset_updater = upd->get_changeset_updater(changeset, uid);

  auto tags = ChangesetXMLParser().process_message(payload);

  changeset = changeset_updater->api_create_changeset(tags);

  output_text = std::to_string(changeset);

  upd->commit();
}

changeset_create_responder::~changeset_create_responder() = default;

changeset_create_handler::changeset_create_handler(request &)
    : payload_enabled_handler(mime::text_plain,
                              http::method::PUT | http::method::OPTIONS) {}

changeset_create_handler::~changeset_create_handler() = default;

std::string changeset_create_handler::log_name() const {
  return ((boost::format("changeset/create")).str());
}

responder_ptr_t
changeset_create_handler::responder(data_selection_ptr &) const {
  throw http::server_error(
      "changeset_create_handler: data_selection unsupported");
}

responder_ptr_t changeset_create_handler::responder(
    data_update_ptr & upd, const std::string &payload, boost::optional<osm_user_id_t> user_id) const {
  return responder_ptr_t(
      new changeset_create_responder(mime_type, upd, payload, user_id));
}

bool changeset_create_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
