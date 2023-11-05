#include "cgimap/config.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"

#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"
#include "cgimap/api06/changeset_upload/osmchange_input_format.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload_handler.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <sstream>


namespace api06 {

changeset_upload_responder::changeset_upload_responder(
    mime::type mt, data_update& upd, osm_changeset_id_t id_, const std::string &payload,
    std::optional<osm_user_id_t> user_id)
    : osm_diffresult_responder(mt) {

  osm_changeset_id_t changeset = id_;
  osm_user_id_t uid = *user_id;

  OSMChange_Tracking change_tracking{};

  auto changeset_updater = upd.get_changeset_updater(changeset, uid);
  auto node_updater = upd.get_node_updater(change_tracking);
  auto way_updater = upd.get_way_updater(change_tracking);
  auto relation_updater = upd.get_relation_updater(change_tracking);

  changeset_updater->lock_current_changeset(true);

  OSMChange_Handler handler(*node_updater, *way_updater, *relation_updater, changeset);

  OSMChangeXMLParser parser(handler);

  parser.process_message(payload);

  // store diffresult for output handling in class osm_diffresult_responder
  m_diffresult = change_tracking.assemble_diffresult();

  const auto new_changes = handler.get_num_changes();

  if (global_settings::get_ratelimiter_upload()) {

    auto max_changes = upd.get_rate_limit(uid);
    if (new_changes > max_changes)
    {
      logger::message(
          fmt::format(
              "Upload of {} changes by user {} in changeset {} blocked due to rate limiting, max. {} changes allowed",
              new_changes, uid, changeset, max_changes));
      throw http::too_many_requests("Upload has been blocked due to rate limiting. Please try again later.");
    }
  }

  changeset_updater->update_changeset(new_changes, handler.get_bbox());

  upd.commit();
}

changeset_upload_handler::changeset_upload_handler(request &,
                                                   osm_changeset_id_t id_)
    : payload_enabled_handler(mime::unspecified_type,
                              http::method::POST | http::method::OPTIONS),
      id(id_) {}

std::string changeset_upload_handler::log_name() const {
  return (fmt::format("changeset/upload {:d}", id));
}

responder_ptr_t
changeset_upload_handler::responder(data_selection &) const {
  throw http::server_error(
      "changeset_upload_handler: data_selection unsupported");
}

responder_ptr_t changeset_upload_handler::responder(
    data_update & upd, const std::string &payload, std::optional<osm_user_id_t> user_id) const {
  return responder_ptr_t(
      new changeset_upload_responder(mime_type, upd, id, payload, user_id));
}

bool changeset_upload_handler::requires_selection_after_update() const {
  return false;
}

} // namespace api06
