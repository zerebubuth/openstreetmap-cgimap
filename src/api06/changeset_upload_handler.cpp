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

using std::stringstream;
using std::vector;
using std::list;
using boost::shared_ptr;
namespace pt = boost::posix_time;

namespace api06 {

changeset_upload_responder::changeset_upload_responder(
    mime::type mt, data_update_ptr & upd, osm_changeset_id_t id_, const std::string &payload,
    boost::optional<osm_user_id_t> user_id)
    : osm_diffresult_responder(mt), id(id_), upd(upd) {

  if (!user_id)
    throw http::unauthorized("User is not authorized to upload changeset");

  osm_changeset_id_t changeset = id_;
  osm_user_id_t uid = *user_id;

  change_tracking = std::make_shared<OSMChange_Tracking>();

  auto changeset_updater = upd->get_changeset_updater(changeset, uid);
  auto node_updater = upd->get_node_updater(change_tracking);
  auto way_updater = upd->get_way_updater(change_tracking);
  auto relation_updater = upd->get_relation_updater(change_tracking);

  changeset_updater->lock_current_changeset();

  OSMChange_Handler handler(std::move(node_updater), std::move(way_updater),
                            std::move(relation_updater), changeset, uid);

  OSMChangeXMLParser parser(&handler);

  parser.process_message(payload);

  change_tracking->populate_orig_sequence_mapping();

  changeset_updater->update_changeset(handler.get_num_changes(),
                                      handler.get_bbox());

  upd->commit();
}

changeset_upload_responder::~changeset_upload_responder() {}

changeset_upload_handler::changeset_upload_handler(request &req,
                                                   osm_changeset_id_t id_)
    : payload_enabled_handler(mime::unspecified_type,
                              http::method::POST | http::method::OPTIONS),
      id(id_) {}

changeset_upload_handler::~changeset_upload_handler() {}

std::string changeset_upload_handler::log_name() const {
  return ((boost::format("changeset/upload %1%") % id).str());
}

responder_ptr_t
changeset_upload_handler::responder(data_selection_ptr &w) const {
  throw http::server_error(
      "changeset_upload_handler: data_selection unsupported");
}

responder_ptr_t changeset_upload_handler::responder(
    data_update_ptr & upd, const std::string &payload, boost::optional<osm_user_id_t> user_id) const {
  return responder_ptr_t(
      new changeset_upload_responder(mime_type, upd, id, payload, user_id));
}

} // namespace api06
