#include "cgimap/backend/apidb/pgsql_update.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/infix_ostream_iterator.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/backend/apidb/transaction_manager.hpp"

#include "cgimap/backend/apidb/changeset_upload/changeset_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"

#include <set>
#include <sstream>
#include <list>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/iterator/transform_iterator.hpp>

#if PQXX_VERSION_MAJOR >= 4
#define PREPARE_ARGS(args)
#else
#define PREPARE_ARGS(args) args
#endif

namespace po = boost::program_options;
namespace pt = boost::posix_time;
using std::set;
using std::list;
using std::vector;
using boost::shared_ptr;

namespace {
std::string connect_db_str(const po::variables_map &options) {
  // build the connection string.
  std::ostringstream ostr;
  ostr << "dbname=" << options["dbname"].as<std::string>();
  if (options.count("host")) {
    ostr << " host=" << options["host"].as<std::string>();
  }
  if (options.count("username")) {
    ostr << " user=" << options["username"].as<std::string>();
  }
  if (options.count("password")) {
    ostr << " password=" << options["password"].as<std::string>();
  }
  if (options.count("dbport")) {
    ostr << " port=" << options["dbport"].as<std::string>();
  }

  return ostr.str();
}

} // anonymous namespace

pgsql_update::pgsql_update(
    pqxx::connection &conn)
    : m{ conn } {

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
        ON COMMIT DROP
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
        ON COMMIT DROP
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
        ON COMMIT DROP
     )");
}

pgsql_update::~pgsql_update() {}

std::unique_ptr<Changeset_Updater>
pgsql_update::get_changeset_updater(osm_changeset_id_t changeset, osm_user_id_t uid)
{
  std::unique_ptr<Changeset_Updater> changeset_updater(new ApiDB_Changeset_Updater(m, changeset, uid));
  return changeset_updater;
}

std::unique_ptr<Node_Updater>
pgsql_update::get_node_updater(std::shared_ptr<OSMChange_Tracking> ct)
{
  std::unique_ptr<Node_Updater> node_updater(new ApiDB_Node_Updater(m, ct));
  return node_updater;
}

std::unique_ptr<Way_Updater>
pgsql_update::get_way_updater(std::shared_ptr<OSMChange_Tracking> ct)
{
  std::unique_ptr<Way_Updater> way_updater(new ApiDB_Way_Updater(m, ct));
  return way_updater;
}

std::unique_ptr<Relation_Updater>
pgsql_update::get_relation_updater(std::shared_ptr<OSMChange_Tracking> ct)
{
  std::unique_ptr<Relation_Updater> relation_updater(new ApiDB_Relation_Updater(m, ct));
  return relation_updater;
}

void pgsql_update::commit() {
  m.commit();
}



pgsql_update::factory::factory(const po::variables_map &opts)
    : m_connection(connect_db_str(opts))
#if PQXX_VERSION_MAJOR >= 4
      ,m_errorhandler(m_connection)
#endif
 {

  check_postgres_version(m_connection);

  // set the connections to use the appropriate charset.
  m_connection.set_client_encoding(opts["charset"].as<std::string>());

  // ignore notice messages
#if PQXX_VERSION_MAJOR < 4
  m_connection.set_noticer(
      std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));
  m_cache_connection.set_noticer(
      std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));
#endif

}

pgsql_update::factory::~factory() {}

boost::shared_ptr<data_update>
pgsql_update::factory::make_data_update() {
  return boost::make_shared<pgsql_update>(boost::ref(m_connection));
}
