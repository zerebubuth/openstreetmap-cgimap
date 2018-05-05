#include "cgimap/api06/changeset_upload_handler.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/config.hpp"

#include "cgimap/infix_ostream_iterator.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload/osmchange_input_format.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/transaction_manager.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"
//#include "cgimap/api06/changeset_upload/osmchange_input_format.hpp"

#include "types.hpp"
#include "util.hpp"

#include <sstream>

using std::stringstream;
using std::vector;
using std::list;
using boost::shared_ptr;
namespace pt = boost::posix_time;


namespace api06 {

changeset_upload_responder::changeset_upload_responder(
  mime::type mt, osm_changeset_id_t id_, data_selection_ptr &w_)
  : osmchange_responder(mt, w_), id(id_) {

 // throw http::server_error("unimplemented");

  int changeset = id_;
  int uid = 1;

  std::string data = "";

  Transaction_Manager m { "dbname=openstreetmap" };

  change_tracking = std::make_shared<OSMChange_Tracking>();

  // TODO: we're still in api06 code, don't use ApiDB_*_Updater here!!

  std::unique_ptr<Changeset_Updater> changeset_updater(new ApiDB_Changeset_Updater(m, changeset, uid));
  std::unique_ptr<Node_Updater> node_updater(new ApiDB_Node_Updater(m, change_tracking));
  std::unique_ptr<Way_Updater> way_updater(new ApiDB_Way_Updater(m, change_tracking));
  std::unique_ptr<Relation_Updater> relation_updater(new ApiDB_Relation_Updater(m, change_tracking));


  changeset_updater->lock_current_changeset();

  OSMChange_Handler handler(std::move(node_updater), std::move(way_updater), std::move(relation_updater),
                  changeset, uid);

  OSMChangeXMLParser parser(&handler);

  parser.process_message(data);

  handler.finish_processing();

  changeset_updater->update_changeset(handler.get_num_changes(), handler.get_bbox());

  m.commit();

}

void changeset_upload_responder::write(shared_ptr<output_formatter> formatter,
                                  const std::string &generator,
                                  const pt::ptime &now) {
  // TODO: is it possible that formatter can be null?
  output_formatter &fmt = *formatter;

  try {
    fmt.start_document(generator, "diffResult");

    // Nodes

    for (const auto& id : change_tracking->created_node_ids)
      fmt.write_diffresult_create_modify(element_type_node, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->modified_node_ids)
      fmt.write_diffresult_create_modify(element_type_node, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->already_deleted_node_ids)
      fmt.write_diffresult_create_modify(element_type_node, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->deleted_node_ids)
      fmt.write_diffresult_delete(element_type_node, id);


    // Ways

    for (const auto& id : change_tracking->created_way_ids)
      fmt.write_diffresult_create_modify(element_type_way, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->modified_way_ids)
      fmt.write_diffresult_create_modify(element_type_way, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->already_deleted_way_ids)
      fmt.write_diffresult_create_modify(element_type_way, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->deleted_way_ids)
      fmt.write_diffresult_delete(element_type_way, id);


    // Relations

    for (const auto& id : change_tracking->created_relation_ids)
      fmt.write_diffresult_create_modify(element_type_relation, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->modified_relation_ids)
      fmt.write_diffresult_create_modify(element_type_relation, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->already_deleted_relation_ids)
      fmt.write_diffresult_create_modify(element_type_relation, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->deleted_relation_ids)
      fmt.write_diffresult_delete(element_type_relation, id);


  } catch (const std::exception &e) {
    fmt.error(e);
  }

  fmt.end_document();
}

changeset_upload_responder::~changeset_upload_responder() {}

changeset_upload_handler::changeset_upload_handler(request &req, osm_changeset_id_t id_)
  : handler(mime::unspecified_type, http::method::POST | http::method::OPTIONS)
  , id(id_) {
}

changeset_upload_handler::~changeset_upload_handler() {}

std::string changeset_upload_handler::log_name() const { return "changeset/upload"; }

responder_ptr_t changeset_upload_handler::responder(data_selection_ptr &w) const {
  return responder_ptr_t(new changeset_upload_responder(mime_type, id, w));
}

} // namespace api06
