#include "cgimap/logger.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/config.hpp"

#include "cgimap/infix_ostream_iterator.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload_handler.hpp"
#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/api06/changeset_upload/osmchange_input_format.hpp"
#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/transaction_manager.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"


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
  mime::type mt, osm_changeset_id_t id_, const std::string & payload, boost::optional<osm_user_id_t> user_id)
  : osm_diffresult_responder(mt), id(id_) {

  if (!user_id)
    throw http::unauthorized("User is not authorized to upload changeset");

  int changeset = id_;
  int uid = *user_id;

  Transaction_Manager m { "dbname=openstreetmap" };

  // ***************************************

  // TODO: Creating temporary tables needs to be moved to a proper location, once it is
  // decided where the db connection for changeset uploads is managed!

  // temporary tables for changeset upload
  m.exec(R"(CREATE TEMPORARY TABLE tmp_create_nodes 
              (
                id bigint NOT NULL DEFAULT nextval('current_nodes_id_seq'::regclass),  
                latitude integer NOT NULL,
                longitude integer NOT NULL,
                changeset_id bigint NOT NULL,
                visible boolean NOT NULL DEFAULT true,
                "timestamp" timestamp without time zone NOT NULL DEFAULT (now() at time zone 'utc'),
                tile bigint NOT NULL,
                version bigint NOT NULL DEFAULT 1,
                old_id bigint NOT NULL UNIQUE,
                PRIMARY KEY (id)) 
              )");

  m.exec(R"(CREATE TEMPORARY TABLE tmp_create_ways 
              (
                id bigint NOT NULL DEFAULT nextval('current_ways_id_seq'::regclass),  
                changeset_id bigint NOT NULL,
                visible boolean NOT NULL DEFAULT true,
                "timestamp" timestamp without time zone NOT NULL DEFAULT (now() at time zone 'utc'),
                version bigint NOT NULL DEFAULT 1,
                old_id bigint NOT NULL UNIQUE,
                PRIMARY KEY (id)) 
              )");

  m.exec(R"(CREATE TEMPORARY TABLE tmp_create_relations 
              (
                id bigint NOT NULL DEFAULT nextval('current_relations_id_seq'::regclass),  
                changeset_id bigint NOT NULL,
                visible boolean NOT NULL DEFAULT true,
                "timestamp" timestamp without time zone NOT NULL DEFAULT (now() at time zone 'utc'),
                version bigint NOT NULL DEFAULT 1,
                old_id bigint NOT NULL UNIQUE,
                PRIMARY KEY (id)) 
              )");

  // ***************************************


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

  parser.process_message(payload);

  handler.finish_processing();

  changeset_updater->update_changeset(handler.get_num_changes(), handler.get_bbox());

  m.commit();
}


changeset_upload_responder::~changeset_upload_responder() {}

changeset_upload_handler::changeset_upload_handler(request &req, osm_changeset_id_t id_)
  : payload_enabled_handler(mime::unspecified_type, http::method::POST | http::method::OPTIONS)
  , id(id_) {
}

changeset_upload_handler::~changeset_upload_handler() {}

std::string changeset_upload_handler::log_name() const { return "changeset/upload"; }

responder_ptr_t changeset_upload_handler::responder(data_selection_ptr &w) const {
  throw http::server_error("changeset_upload_handler: data_selection unsupported");
}

responder_ptr_t changeset_upload_handler::responder(const std::string & payload,
                              boost::optional<osm_user_id_t> user_id) const {
  return responder_ptr_t(new changeset_upload_responder(mime_type, id, payload, user_id));
}

} // namespace api06
