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

#include <functional>
#include <set>
#include <sstream>
#include <list>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <boost/iterator/transform_iterator.hpp>


namespace po = boost::program_options;
using std::set;
using std::list;
using std::vector;
using std::shared_ptr;

namespace {
std::string connect_db_str(const po::variables_map &options) {
  // build the connection string.
  std::ostringstream ostr;

  if (options.count("dbname") == 0 &&
      options.count("update-dbname") == 0) {
    throw std::runtime_error("Must provide either one of --dbname or "
                             "--update-dbname to configure database "
                             "name for update (API write) connections.");
  }

#define CONNOPT(a,b)                                                    \
  if (options.count("update-" a)) {                                     \
    ostr << " " << (b "=") << options["update-" a].as<std::string>();   \
  } else if (options.count(a)) {                                        \
    ostr << " " << (b "=") << options[a].as<std::string>();             \
  }

  CONNOPT("dbname", "dbname");
  CONNOPT("host", "host");
  CONNOPT("username", "user");
  CONNOPT("password", "password");
  CONNOPT("dbport", "port");

#undef CONNOPT
  return ostr.str();
}

} // anonymous namespace

pgsql_update::pgsql_update(
    pqxx::connection &conn, bool readonly)
    : m{ conn }, m_readonly{ readonly } {

  if (is_api_write_disabled())
    return;

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

pgsql_update::~pgsql_update() = default;

bool pgsql_update::is_api_write_disabled() {
  return m_readonly;
}

std::unique_ptr<api06::Changeset_Updater>
pgsql_update::get_changeset_updater(osm_changeset_id_t changeset, osm_user_id_t uid)
{
  std::unique_ptr<api06::Changeset_Updater> changeset_updater(new ApiDB_Changeset_Updater(m, changeset, uid));
  return changeset_updater;
}

std::unique_ptr<api06::Node_Updater>
pgsql_update::get_node_updater(std::shared_ptr<api06::OSMChange_Tracking> ct)
{
  std::unique_ptr<api06::Node_Updater> node_updater(new ApiDB_Node_Updater(m, ct));
  return node_updater;
}

std::unique_ptr<api06::Way_Updater>
pgsql_update::get_way_updater(std::shared_ptr<api06::OSMChange_Tracking> ct)
{
  std::unique_ptr<api06::Way_Updater> way_updater(new ApiDB_Way_Updater(m, ct));
  return way_updater;
}

std::unique_ptr<api06::Relation_Updater>
pgsql_update::get_relation_updater(std::shared_ptr<api06::OSMChange_Tracking> ct)
{
  std::unique_ptr<api06::Relation_Updater> relation_updater(new ApiDB_Relation_Updater(m, ct));
  return relation_updater;
}

void pgsql_update::commit() {
  m.commit();
}


pgsql_update::factory::factory(const po::variables_map &opts)
    : m_connection(connect_db_str(opts)), m_api_write_disabled(false)
      ,m_errorhandler(m_connection)
 {

  check_postgres_version(m_connection);

  // set the connections to use the appropriate charset.
  std::string db_charset = opts["charset"].as<std::string>();
  if (opts.count("update-charset")) {
     db_charset = opts["update-charset"].as<std::string>();
  }
  m_connection.set_client_encoding(db_charset);

  // set the connection to readonly transaction, if disable-api-write flag is set
  if (opts.count("disable-api-write") != 0) {
    m_api_write_disabled = true;
    m_connection.set_variable("default_transaction_read_only", "true");
  }
}

pgsql_update::factory::~factory() = default;

std::shared_ptr<data_update>
pgsql_update::factory::make_data_update() {
  return std::make_shared<pgsql_update>(std::ref(m_connection), m_api_write_disabled);
}
