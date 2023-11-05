#include "cgimap/api06/node_create_handler.hpp"

namespace api06 {

node_create_responder::node_create_responder(
    mime::type mt, data_update & upd, const std::string &payload,
    std::optional<osm_user_id_t> user_id)
    : text_responder(mt) {
  OSMChange_Tracking change_tracking{};
  osm_changeset_id_t cid = 1; // TODO get changeset id
  osm_user_id_t uid = *user_id;
  auto changeset_updater = upd.get_changeset_updater(cid, uid);
  auto node_updater = upd.get_node_updater(change_tracking);

  changeset_updater->lock_current_changeset(true);
  std::map<std::string, std::string> tags;
  node_updater->add_node(12, 34, cid, -1, tags); // TODO get lat, lon, tags
  node_updater->process_new_nodes();
  changeset_updater->update_changeset(node_updater->get_num_changes(),
                                      node_updater->bbox());
  upd.commit();

  auto oid = change_tracking.created_node_ids[0].new_id;
  output_text = std::to_string(oid);
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
