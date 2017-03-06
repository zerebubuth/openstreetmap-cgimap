#include "cgimap/backend/pgsnapshot/snapshot_selection.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include <set>
#include <sstream>
#include <list>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>

#if PQXX_VERSION_MAJOR >= 4
#define PREPARE_ARGS(args)
#else
#define PREPARE_ARGS(args) args
#endif

// Unlike apidb, SCALE is purely internal to the SQL in this file and
// aren't part of the pgsnapshot schema. It saves sending around postgis
// points, and they need to be converted to lat/lon at some point anyways
#define SCALE "10000000"

namespace po = boost::program_options;
using std::set;
using std::stringstream;
using std::vector;

namespace pqxx {
template <> struct string_traits<vector<osm_nwr_id_t> > {
  static const char *name() { return "vector<osm_nwr_id_t>"; }
  static bool has_null() { return false; }
  static bool is_null(const vector<osm_nwr_id_t> &) { return false; }
  static stringstream null() {
    internal::throw_null_conversion(name());
    // No, dear compiler, we don't need a return here.
    throw 0;
  }
  static void from_string(const char[], vector<osm_nwr_id_t> &) {}
  static std::string to_string(const vector<osm_nwr_id_t> &ids) {
    stringstream ostr;
    ostr << "{";
    std::copy(ids.begin(), ids.end(),
              infix_ostream_iterator<osm_nwr_id_t>(ostr, ","));
    ostr << "}";
    return ostr.str();
  }
};
}

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

  return ostr.str();
}

void extract_elem(const pqxx::result::tuple &row, element_info &elem) {
  elem.id = row["id"].as<osm_nwr_id_t>();
  elem.version = row["version"].as<int>();
  elem.timestamp = row["timestamp"].c_str();
  elem.changeset = row["changeset_id"].as<osm_changeset_id_t>();
  if (!row["uid"].is_null()) {
    elem.uid = row["uid"].as<osm_user_id_t>();
  } else {
    elem.uid = boost::none;
  }
  if (!row["display_name"].is_null()) {
    elem.display_name = row["display_name"].c_str();
  } else {
    elem.display_name = boost::none;
  }
  elem.visible = true;
}

void extract_tags(const pqxx::result &res, tags_t &tags) {
  tags.clear();
  for (pqxx::result::const_iterator itr = res.begin(); itr != res.end();
       ++itr) {
    tags.push_back(std::make_pair(std::string((*itr)["k"].c_str()),
                                  std::string((*itr)["v"].c_str())));
  }
}

void extract_nodes(const pqxx::result &res, nodes_t &nodes) {
  nodes.clear();
  for (pqxx::result::const_iterator itr = res.begin(); itr != res.end();
       ++itr) {
    nodes.push_back((*itr)[0].as<osm_nwr_id_t>());
  }
}

element_type type_from_name(const char *name) {
  switch (name[0]) {
  case 'N':
  case 'n':
    return element_type_node;

  case 'W':
  case 'w':
    return element_type_way;

  case 'R':
  case 'r':
    return element_type_relation;
  }

  throw std::runtime_error("Unable to determine element type from name.");
}

void extract_members(const pqxx::result &res, members_t &members) {
  member_info member;
  members.clear();
  for (pqxx::result::const_iterator itr = res.begin(); itr != res.end();
       ++itr) {
    member.type = type_from_name((*itr)["member_type"].c_str());
    member.ref = (*itr)["member_id"].as<osm_nwr_id_t>();
    member.role = (*itr)["member_role"].c_str();
    members.push_back(member);
  }
}

} // anonymous namespace

snapshot_selection::snapshot_selection(pqxx::connection &conn) : w(conn) {
  w.set_variable("default_transaction_read_only", "false");
  w.exec("CREATE TEMPORARY TABLE tmp_nodes ("
         "id bigint NOT NULL PRIMARY KEY,"
         "version integer NOT NULL,"
         "user_id integer NOT NULL,"
         "tstamp timestamp without time zone NOT NULL,"
         "changeset_id bigint NOT NULL,"
         "tags hstore,"
         "lon integer NOT NULL," // 2x4 byte integers to save sending around 30
                                 // bytes of geometry
         "lat integer NOT NULL)");

  w.exec("CREATE TEMPORARY TABLE tmp_ways ( "
         "id bigint NOT NULL PRIMARY KEY, "
         "version integer NOT NULL, "
         "user_id integer NOT NULL, "
         "tstamp timestamp without time zone NOT NULL, "
         "changeset_id bigint NOT NULL, "
         "tags hstore, "
         "nodes bigint[])"); // pgsnapshot could include a bbox or linestring
                             // column, but not needed here
  w.exec("CREATE TEMPORARY TABLE tmp_relations ( "
         "id bigint NOT NULL PRIMARY KEY, "
         "version integer NOT NULL, "
         "user_id integer NOT NULL, "
         "tstamp timestamp without time zone NOT NULL, "
         "changeset_id bigint NOT NULL, "
         "tags hstore)");
  w.set_variable("default_transaction_read_only", "true");
}

snapshot_selection::~snapshot_selection() {}

void snapshot_selection::write_nodes(output_formatter &formatter) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  logger::message("Fetching nodes");
  element_info elem;
  tags_t tags;

  pqxx::result nodes = w.prepared("extract_nodes").exec();
  for (pqxx::result::const_iterator itr = nodes.begin(); itr != nodes.end();
       ++itr) {
    extract_elem(*itr, elem);
    if (itr["untagged"].as<bool>()) {
      tags.clear();
    } else {
      extract_tags(w.prepared("extract_node_tags")(elem.id).exec(), tags);
    }
    formatter.write_node(elem, (*itr)["lon"].as<double>(),
                         (*itr)["lat"].as<double>(), tags);
  }
}

void snapshot_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");
  element_info elem;
  nodes_t nodes;
  tags_t tags;

  pqxx::result ways = w.prepared("extract_ways").exec();
  for (pqxx::result::const_iterator itr = ways.begin(); itr != ways.end();
       ++itr) {
    extract_elem(*itr, elem);
    extract_nodes(w.prepared("extract_way_nds")(elem.id).exec(), nodes);
    extract_tags(w.prepared("extract_way_tags")(elem.id).exec(), tags);
    formatter.write_way(elem, nodes, tags);
  }
}

void snapshot_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");
  element_info elem;
  members_t members;
  tags_t tags;

  pqxx::result relations = w.prepared("extract_relations").exec();
  for (pqxx::result::const_iterator itr = relations.begin();
       itr != relations.end(); ++itr) {
    extract_elem(*itr, elem);
    extract_members(w.prepared("extract_relation_members")(elem.id).exec(),
                    members);
    extract_tags(w.prepared("extract_relation_tags")(elem.id).exec(), tags);
    formatter.write_relation(elem, members, tags);
  }
}

data_selection::visibility_t
    snapshot_selection::check_node_visibility(osm_nwr_id_t) {
  return data_selection::exists;
}

data_selection::visibility_t
    snapshot_selection::check_way_visibility(osm_nwr_id_t) {
  return data_selection::exists;
}

data_selection::visibility_t
    snapshot_selection::check_relation_visibility(osm_nwr_id_t) {
  return data_selection::exists;
}

int snapshot_selection::select_nodes(const std::vector<osm_nwr_id_t> &ids) {
  return w.prepared("add_nodes_list")(ids).exec().affected_rows();
}

int snapshot_selection::select_ways(const std::vector<osm_nwr_id_t> &ids) {
  return w.prepared("add_ways_list")(ids).exec().affected_rows();
}

int snapshot_selection::select_relations(const std::vector<osm_nwr_id_t> &ids) {
  return w.prepared("add_relations_list")(ids).exec().affected_rows();
}

int snapshot_selection::select_nodes_from_bbox(const bbox &bounds,
                                               int max_nodes) {
  return w.prepared("nodes_from_bbox")(bounds.minlon)(bounds.minlat)(
               bounds.maxlon)(bounds.maxlat)(max_nodes + 1)
      .exec()
      .affected_rows();
}

void snapshot_selection::select_nodes_from_relations() {
  logger::message("Filling tmp_nodes (from relations)");

  w.prepared("nodes_from_relations").exec();
}

void snapshot_selection::select_ways_from_nodes() {
  logger::message("Filling tmp_ways (from nodes)");

  w.prepared("ways_from_nodes").exec();
}

void snapshot_selection::select_ways_from_relations() {
  logger::message("Filling tmp_ways (from relations)");
  w.prepared("ways_from_relations").exec();
}

void snapshot_selection::select_relations_from_ways() {
  logger::message("Filling tmp_relations (from ways)");

  w.prepared("relations_from_ways").exec();
}

void snapshot_selection::select_nodes_from_way_nodes() {
  w.prepared("nodes_from_way_nodes").exec();
}

void snapshot_selection::select_relations_from_nodes() {
  w.prepared("relations_from_nodes").exec();
}

void snapshot_selection::select_relations_from_way_nodes() {
  w.prepared("relations_from_way_nodes").exec();
}

void snapshot_selection::select_relations_from_relations() {
  w.prepared("relations_from_relations").exec();
}

void snapshot_selection::select_relations_members_of_relations() {
  w.prepared("relation_members_of_relations").exec();
}

snapshot_selection::factory::factory(const po::variables_map &opts)
    : m_connection(connect_db_str(opts))
#if PQXX_VERSION_MAJOR >= 4
    , m_errorhandler(m_connection)
#endif
{
  if (m_connection.server_version() < 90300) {
    throw std::runtime_error("Expected Postgres version 9.3+, currently installed version "
        + std::to_string(m_connection.server_version()));
  }

  // set the connections to use the appropriate charset.
  m_connection.set_client_encoding(opts["charset"].as<std::string>());

  // ignore notice messages
#if PQXX_VERSION_MAJOR < 4
  m_connection.set_noticer(
      std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));
#endif

  logger::message("Preparing prepared statements.");

  // clang-format off

  // extraction functions for getting the data back out when the
  // selection set has been built up.
  m_connection.prepare("extract_nodes",
    "SELECT n.id, n.version, "
        "to_char(n.tstamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "n.changeset_id, nullif(n.user_id,-1) AS uid, u.name AS display_name, "
        "lon / " SCALE ". AS lon, lat / " SCALE ". AS lat, "
        "tags = ''::hstore AS untagged " // Special-case untagged nodes
      "FROM tmp_nodes n "
        "LEFT JOIN users u ON (n.user_id = u.id)");
  m_connection.prepare("extract_node_tags",
    "SELECT (each(tags)).key AS k, (each(tags)).value AS v "
      "FROM tmp_nodes WHERE id=$1")
    PREPARE_ARGS(("bigint"));

  m_connection.prepare("extract_ways",
    "SELECT w.id, w.version, "
        "to_char(w.tstamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "w.changeset_id, nullif(w.user_id,-1) AS uid, u.name AS display_name "
      "FROM tmp_ways w "
        "LEFT JOIN users u ON (w.user_id = u.id)");
  m_connection.prepare("extract_way_nds",
    "SELECT unnest(nodes) FROM tmp_ways "
      "WHERE id=$1")
    PREPARE_ARGS(("bigint"));
  m_connection.prepare("extract_way_tags",
    "SELECT (each(tags)).key AS k, "
        "(each(tags)).value AS v "
      "FROM tmp_ways "
      "WHERE id=$1")
    PREPARE_ARGS(("bigint"));

  m_connection.prepare("extract_relations",
    "SELECT r.id, r.version, "
        "to_char(r.tstamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "r.changeset_id, nullif(r.user_id,-1) AS uid, u.name AS display_name "
      "FROM tmp_relations r "
        "LEFT JOIN users u ON (r.user_id = u.id)");
  m_connection.prepare("extract_relation_members",
    "SELECT member_type, member_id, member_role "
      "FROM relation_members "
      "WHERE relation_id=$1 "
      "ORDER BY sequence_id ASC")
    PREPARE_ARGS(("bigint"));
  m_connection.prepare("extract_relation_tags",
    "SELECT (each(tags)).key AS k, (each(tags)).value as v "
      "FROM tmp_relations "
      "WHERE id=$1")
    PREPARE_ARGS(("bigint"));

  // map? call geometry stuff
  m_connection.prepare("nodes_from_bbox",
    "INSERT INTO tmp_nodes "
      "SELECT n.id, n.version, n.user_id, n.tstamp, n.changeset_id, n.tags, "
          "ST_X(n.geom) * " SCALE ", ST_Y(n.geom) * " SCALE " "
        "FROM nodes n "
        "WHERE geom && ST_SetSRID(ST_MakeBox2D(ST_Point($1,$2),ST_Point($3,$4)),4326) "
        "AND id NOT IN (SELECT id FROM tmp_nodes) "
        "LIMIT $5")
    PREPARE_ARGS(("double precision")("double precision")("double precision")("double precision")("integer"));

  // selecting elements from a list
  m_connection.prepare("add_nodes_list",
    "INSERT INTO tmp_nodes "
      "SELECT id,version,user_id,tstamp,changeset_id,tags, "
        "ST_X(geom) * " SCALE ", ST_Y(geom) * " SCALE
        "FROM nodes WHERE id = ANY($1) "
        "AND id NOT IN (SELECT id FROM tmp_nodes)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("add_ways_list",
    "INSERT INTO tmp_ways "
      "SELECT id,version,user_id,tstamp,changeset_id,tags,nodes "
        "FROM ways "
        "WHERE id = ANY($1) "
          "AND id NOT IN (SELECT id FROM tmp_ways)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("add_relations_list",
    "INSERT INTO tmp_relations "
      "SELECT id,version,user_id,tstamp,changeset_id,tags "
        "FROM relations "
        "WHERE id = ANY($1) "
          "AND id NOT IN (SELECT id FROM tmp_relations)")
    PREPARE_ARGS(("bigint[]"));

  // queries for filling elements which are used as members in relations
  m_connection.prepare("nodes_from_relations",
    "INSERT INTO tmp_nodes "
      "SELECT DISTINCT n.id,n.version,n.user_id,n.tstamp,n.changeset_id,n.tags, "
          "ST_X(geom) * " SCALE ", ST_Y(geom) * " SCALE
        "FROM tmp_relations r "
          "JOIN relation_members rm ON (r.id = rm.relation_id) "
          "JOIN nodes n ON (rm.member_type = 'N' AND rm.member_id = n.id) "
        "WHERE rm.member_id NOT IN (SELECT id FROM tmp_nodes)");
  m_connection.prepare("ways_from_relations",
    "INSERT INTO tmp_ways "
      "SELECT DISTINCT w.id,w.version,w.user_id,w.tstamp,w.changeset_id,w.tags,w.nodes "
        "FROM tmp_relations r "
          "JOIN relation_members rm ON (r.id = rm.relation_id) "
          "JOIN ways w ON (rm.member_type = 'W' AND rm.member_id = w.id) "
        "WHERE rm.member_id NOT IN (SELECT id FROM tmp_ways)");
  m_connection.prepare("relation_members_of_relations",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT r.id,r.version,r.user_id,r.tstamp,r.changeset_id,r.tags  "
        "FROM tmp_relations tr "
          "JOIN relation_members rm ON (tr.id = rm.relation_id) "
          "JOIN relations r ON (rm.member_type = 'R' AND rm.member_id = r.id) "
        "WHERE rm.member_id NOT IN (SELECT id FROM tmp_relations)");

  // select ways which use nodes already in the working set
    m_connection.prepare("ways_from_nodes",
      "INSERT INTO tmp_ways "
        "SELECT w.id,w.version,w.user_id,w.tstamp,w.changeset_id,w.tags,w.nodes "
          "FROM "
            "(SELECT DISTINCT wn.way_id AS way_id "
              "FROM tmp_nodes n JOIN way_nodes wn ON (n.id = wn.node_id) "
                "LEFT JOIN tmp_ways tw ON (wn.way_id = tw.id) "
              "WHERE tw.id IS NULL) AS wn "
            "JOIN ways w ON (wn.way_id = w.id)");

  // select nodes used by ways already in the working set
  m_connection.prepare("nodes_from_way_nodes",
    "INSERT INTO tmp_nodes "
      "SELECT n.id,n.version,n.user_id,n.tstamp,n.changeset_id,n.tags, "
          "ST_X(n.geom) * 10000000, ST_Y(n.geom) * 10000000 "
        "FROM (SELECT DISTINCT unnest(nodes) AS node_id FROM tmp_ways) AS wn "
          "JOIN nodes n ON (wn.node_id = n.id) "
        "WHERE node_id NOT IN (SELECT id FROM tmp_nodes)");
  // selecting relations which have members which are already in
  // the working set.
  m_connection.prepare("relations_from_ways",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT r.id,r.version,r.user_id,r.tstamp,r.changeset_id,r.tags "
        "FROM tmp_ways w "
          "JOIN relation_members rm ON (rm.member_type = 'W' AND rm.member_id = w.id) "
          "LEFT JOIN tmp_relations tr ON (rm.relation_id = tr.id) "
          "JOIN relations r ON (rm.relation_id = r.id) "
        "WHERE tr.id IS NULL");
  m_connection.prepare("relations_from_nodes",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT r.id,r.version,r.user_id,r.tstamp,r.changeset_id,r.tags "
        "FROM tmp_nodes n "
        "JOIN relation_members rm ON (rm.member_type = 'N' AND rm.member_id = n.id) "
          "LEFT JOIN tmp_relations tr ON (rm.relation_id = tr.id) "
          "JOIN relations r ON (rm.relation_id = r.id) "
        "WHERE tr.id IS NULL");
  m_connection.prepare("relations_from_relations",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT r.id,r.version,r.user_id,r.tstamp,r.changeset_id,r.tags "
        "FROM tmp_relations tr "
        "JOIN relation_members rm ON (rm.member_type = 'R' AND rm.member_id = tr.id) "
          "LEFT JOIN tmp_relations trn ON (rm.relation_id = trn.id) "
          "JOIN relations r ON (rm.relation_id = r.id) "
        "WHERE trn.id IS NULL");

  // clang-format on
}

snapshot_selection::factory::~factory() {}

boost::shared_ptr<data_selection>
snapshot_selection::factory::make_selection() {
  return boost::make_shared<snapshot_selection>(boost::ref(m_connection));
}
