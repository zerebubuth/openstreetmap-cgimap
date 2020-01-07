#include "cgimap/config.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"

#include "cgimap/api06/changeset_update_handler.hpp"
#include "cgimap/api06/changeset_upload/changeset_input_format.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <sstream>


namespace api06 {

changeset_update_responder::changeset_update_responder(
    mime::type mt,
    data_update_ptr & upd,
    osm_changeset_id_t changeset_id,
    const std::string &payload,
    boost::optional<osm_user_id_t> user_id)
    : text_responder(mt),
      id(changeset_id){

  osm_user_id_t uid = *user_id;

  auto changeset_updater = upd->get_changeset_updater(changeset_id, uid);

  auto tags = ChangesetXMLParser().process_message(payload);

  changeset_updater->api_update_changeset(tags);

  upd->commit();
}

changeset_update_sel_responder::changeset_update_sel_responder(
    mime::type mt,
    data_selection_ptr & sel,
    osm_changeset_id_t changeset_id)
    : osm_current_responder(mt, sel),
      sel(sel),
      id(changeset_id),
      include_discussion(false){

  sel->select_changesets({changeset_id});
}


changeset_update_responder::~changeset_update_responder() = default;

changeset_update_sel_responder::~changeset_update_sel_responder() = default;

changeset_update_handler::changeset_update_handler(request &req, osm_changeset_id_t id_)
    : payload_enabled_handler(mime::text_xml,
                              http::method::PUT | http::method::OPTIONS),
      id(id_) {}

changeset_update_handler::~changeset_update_handler() = default;

std::string changeset_update_handler::log_name() const {
  return ((boost::format("changeset/update %1%") % id).str());
}

responder_ptr_t
changeset_update_handler::responder(data_selection_ptr &sel) const {
  return responder_ptr_t(
      new changeset_update_sel_responder(mime_type, sel, id));
}

responder_ptr_t changeset_update_handler::responder(
    data_update_ptr & upd, const std::string &payload, boost::optional<osm_user_id_t> user_id) const {
  return responder_ptr_t(
      new changeset_update_responder(mime_type, upd, id, payload, user_id));
}

bool changeset_update_handler::requires_selection_after_update() const {
  return true;
}

} // namespace api06
