#include "cgimap/api06/node_create_handler.hpp"

namespace api06 {

node_create_responder::node_create_responder(
    mime::type mt, data_update & upd, const std::string &payload,
    std::optional<osm_user_id_t> user_id)
    : text_responder(mt) {
  output_text = "1234567";
}

node_create_handler::node_create_handler(request &)
    : payload_enabled_handler(mime::text_plain,
                              http::method::PUT | http::method::OPTIONS) {}

std::string node_create_handler::log_name() const {
  return "node/create";
}

responder_ptr_t
node_create_handler::responder(data_selection &) const {
  throw http::server_error(
      "node_create_handler: data_selection unsupported");
}

responder_ptr_t node_create_handler::responder(
    data_update & upd, const std::string &payload, std::optional<osm_user_id_t> user_id) const {
  return responder_ptr_t(
      new node_create_responder(mime_type, upd, payload, user_id));
}

bool node_create_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
