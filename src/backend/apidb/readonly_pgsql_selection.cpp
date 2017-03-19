#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
#include "cgimap/backend/apidb/common_pgsql_selection.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include <sstream>
#include <list>
#include <vector>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#if PQXX_VERSION_MAJOR >= 4
#define PREPARE_ARGS(args)
#else
#define PREPARE_ARGS(args) args
#endif

namespace po = boost::program_options;
namespace pt = boost::posix_time;
using std::set;
using std::stringstream;
using std::list;
using std::vector;
using boost::shared_ptr;

// number of nodes to chunk together
#define STRIDE (1000)

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

inline data_selection::visibility_t
check_table_visibility(pqxx::work &w, osm_nwr_id_t id,
                       const std::string &prepared_name) {
  pqxx::result res = w.prepared(prepared_name)(id).exec();

  if (res.size() > 0) {
    if (res[0][0].as<bool>()) {
      return data_selection::exists;
    } else {
      return data_selection::deleted;
    }
  } else {
    return data_selection::non_exist;
  }
}

template <typename T> T id_of(const pqxx::tuple &);

template <>
osm_nwr_id_t id_of<osm_nwr_id_t>(const pqxx::tuple &row) {
  return row["id"].as<osm_nwr_id_t>();
}

template <>
osm_changeset_id_t id_of<osm_changeset_id_t>(const pqxx::tuple &row) {
  return row["id"].as<osm_changeset_id_t>();
}

template <>
osm_edition_t id_of<osm_edition_t>(const pqxx::tuple &row) {
  osm_nwr_id_t id = row["id"].as<osm_nwr_id_t>();
  osm_version_t ver = row["version"].as<osm_version_t>();
  return osm_edition_t(id, ver);
}

template <typename T>
inline int insert_results(const pqxx::result &res, set<T> &elems) {
  int num_inserted = 0;

  for (pqxx::result::const_iterator itr = res.begin(); itr != res.end();
       ++itr) {
    const pqxx::tuple &row = *itr;
    const T id = id_of<T>(row);

    // note: only count the *new* rows inserted.
    if (elems.insert(id).second) {
      ++num_inserted;
    }
  }

  return num_inserted;
}

/* Shim for functions not yet converted to prepared statements */
inline int insert_results_of(pqxx::work &w, std::stringstream &query,
                             set<osm_nwr_id_t> &elems) {
  return insert_results(w.exec(query), elems);
}

// use this to remove current versions from the historical set of elements.
struct erase_formatter
  : public output_formatter {

  erase_formatter(
    output_formatter &fmt,
    std::set<osm_edition_t> &sel_historic_nodes,
    std::set<osm_edition_t> &sel_historic_ways,
    std::set<osm_edition_t> &sel_historic_relations)
    : m_fmt(fmt)
    , m_sel_historic_nodes(sel_historic_nodes)
    , m_sel_historic_ways(sel_historic_ways)
    , m_sel_historic_relations(sel_historic_relations) {
  }
  virtual ~erase_formatter() {}

  mime::type mime_type() const { return m_fmt.mime_type(); }

  void start_document(
    const std::string &generator, const std::string &root_name) {
    m_fmt.start_document(generator, root_name);
  }

  void end_document() { m_fmt.end_document(); }
  void error(const std::exception &e) { m_fmt.error(e); }
  void write_bounds(const bbox &bounds) { m_fmt.write_bounds(bounds); }
  void start_element_type(element_type type) { m_fmt.start_element_type(type); }
  void end_element_type(element_type type) { m_fmt.end_element_type(type); }
  void start_action(action_type type) { m_fmt.start_action(type); }
  void end_action(action_type type) { m_fmt.end_action(type); }
  void flush() { m_fmt.flush(); }
  void error(const std::string &str) { m_fmt.error(str); }

  void write_node(
    const element_info &elem, double lon, double lat, const tags_t &tags) {

    m_sel_historic_nodes.erase(osm_edition_t(elem.id, elem.version));
    m_fmt.write_node(elem, lon, lat, tags);
  }

  void write_way(
    const element_info &elem, const nodes_t &nodes, const tags_t &tags) {

    m_sel_historic_ways.erase(osm_edition_t(elem.id, elem.version));
    m_fmt.write_way(elem, nodes, tags);
  }

  void write_relation(
    const element_info &elem, const members_t &members, const tags_t &tags) {

    m_sel_historic_relations.erase(osm_edition_t(elem.id, elem.version));
    m_fmt.write_relation(elem, members, tags);
  }

  void write_changeset(
    const changeset_info &elem,
    const tags_t &tags,
    bool include_comments,
    const comments_t &comments,
    const boost::posix_time::ptime &now) {
    m_fmt.write_changeset(elem, tags, include_comments, comments, now);
  }

private:
  output_formatter &m_fmt;
  std::set<osm_edition_t> &m_sel_historic_nodes, &m_sel_historic_ways,
    &m_sel_historic_relations;
};

} // anonymous namespace

readonly_pgsql_selection::readonly_pgsql_selection(
    pqxx::connection &conn, cache<osm_changeset_id_t, changeset> &changeset_cache)
    : w(conn), cc(changeset_cache)
    , include_changeset_discussions(false)
    , m_redactions_visible(false) {}

readonly_pgsql_selection::~readonly_pgsql_selection() {}

void readonly_pgsql_selection::write_nodes(output_formatter &formatter) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  logger::message("Fetching nodes");
  if (!sel_nodes.empty()) {
    erase_formatter eraser(formatter, sel_historic_nodes, sel_historic_ways,
      sel_historic_relations);
    extract_nodes(w.prepared("extract_nodes")(sel_nodes).exec(), eraser, cc);
  }
  if (!sel_historic_nodes.empty()) {
    std::vector<osm_nwr_id_t> ids, versions;
    for (const auto &ed : sel_historic_nodes) {
      ids.emplace_back(ed.first);
      versions.emplace_back(ed.second);
    }
    extract_nodes(w.prepared("extract_historic_nodes")(ids)(versions).exec(), formatter, cc);
  }
}

void readonly_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");
  if (!sel_ways.empty()) {
    erase_formatter eraser(formatter, sel_historic_nodes, sel_historic_ways,
      sel_historic_relations);
    extract_ways(w.prepared("extract_ways")(sel_ways).exec(), eraser, cc);
  }
  if (!sel_historic_ways.empty()) {
    std::vector<osm_nwr_id_t> ids, versions;
    for (const auto &ed : sel_historic_ways) {
      ids.emplace_back(ed.first);
      versions.emplace_back(ed.second);
    }
    extract_ways(w.prepared("extract_historic_ways")(ids)(versions).exec(), formatter, cc);
  }
}

void readonly_pgsql_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");
  if (!sel_relations.empty()) {
    erase_formatter eraser(formatter, sel_historic_nodes, sel_historic_ways,
      sel_historic_relations);
    extract_relations(w.prepared("extract_relations")(sel_relations).exec(), eraser, cc);
  }
  if (!sel_historic_relations.empty()) {
    std::vector<osm_nwr_id_t> ids, versions;
    for (const auto &ed : sel_historic_relations) {
      ids.emplace_back(ed.first);
      versions.emplace_back(ed.second);
    }
    extract_relations(w.prepared("extract_historic_relations")(ids)(versions).exec(), formatter, cc);
  }
}

void readonly_pgsql_selection::write_changesets(output_formatter &formatter,
                                                const pt::ptime &now) {
  pqxx::result changesets = w.prepared("extract_changesets")(sel_changesets).exec();
  extract_changesets(changesets, formatter, cc, now, include_changeset_discussions);
}

data_selection::visibility_t
readonly_pgsql_selection::check_node_visibility(osm_nwr_id_t id) {
  return check_table_visibility(w, id, "visible_node");
}

data_selection::visibility_t
readonly_pgsql_selection::check_way_visibility(osm_nwr_id_t id) {
  return check_table_visibility(w, id, "visible_way");
}

data_selection::visibility_t
readonly_pgsql_selection::check_relation_visibility(osm_nwr_id_t id) {
  return check_table_visibility(w, id, "visible_relation");
}

int readonly_pgsql_selection::select_nodes(const std::vector<osm_nwr_id_t> &ids) {
  if (!ids.empty()) {
    return insert_results(w.prepared("select_nodes")(ids).exec(), sel_nodes);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_ways(const std::vector<osm_nwr_id_t> &ids) {
  if (!ids.empty()) {
    return insert_results(w.prepared("select_ways")(ids).exec(), sel_ways);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_relations(const std::vector<osm_nwr_id_t> &ids) {
  if (!ids.empty()) {
    return insert_results(w.prepared("select_relations")(ids).exec(),
                          sel_relations);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_nodes_from_bbox(const bbox &bounds,
                                                     int max_nodes) {
  const std::vector<tile_id_t> tiles = tiles_for_area(
      bounds.minlat, bounds.minlon, bounds.maxlat, bounds.maxlon);

  // hack around problem with postgres' statistics, which was
  // making it do seq scans all the time on smaug...
  w.exec("set enable_mergejoin=false");
  w.exec("set enable_hashjoin=false");

  return insert_results(
      w.prepared("visible_node_in_bbox")(tiles)(int(bounds.minlat * SCALE))(
            int(bounds.maxlat * SCALE))(int(bounds.minlon * SCALE))(
            int(bounds.maxlon * SCALE))(max_nodes + 1).exec(),
      sel_nodes);
}

void readonly_pgsql_selection::select_nodes_from_relations() {
  logger::message("Filling sel_nodes (from relations)");

  if (!sel_relations.empty()) {
    insert_results(w.prepared("nodes_from_relations")(sel_relations).exec(),
                   sel_nodes);
  }
}

void readonly_pgsql_selection::select_ways_from_nodes() {
  logger::message("Filling sel_ways (from nodes)");

  if (!sel_nodes.empty()) {
    insert_results(w.prepared("ways_from_nodes")(sel_nodes).exec(), sel_ways);
  }
}

void readonly_pgsql_selection::select_ways_from_relations() {
  logger::message("Filling sel_ways (from relations)");

  if (!sel_relations.empty()) {
    insert_results(w.prepared("ways_from_relations")(sel_relations).exec(),
                   sel_ways);
  }
}

void readonly_pgsql_selection::select_relations_from_ways() {
  logger::message("Filling sel_relations (from ways)");

  if (!sel_ways.empty()) {
    insert_results(w.prepared("relation_parents_of_ways")(sel_ways).exec(),
                   sel_relations);
  }
}

void readonly_pgsql_selection::select_nodes_from_way_nodes() {
  if (!sel_ways.empty()) {
    insert_results(w.prepared("nodes_from_ways")(sel_ways).exec(), sel_nodes);
  }
}

void readonly_pgsql_selection::select_relations_from_nodes() {
  if (!sel_nodes.empty()) {
    insert_results(w.prepared("relation_parents_of_nodes")(sel_nodes).exec(),
                   sel_relations);
  }
}

void readonly_pgsql_selection::select_relations_from_relations() {
  if (!sel_relations.empty()) {
    insert_results(
        w.prepared("relation_parents_of_relations")(sel_relations).exec(),
        sel_relations);
  }
}

void readonly_pgsql_selection::select_relations_members_of_relations() {
  if (!sel_relations.empty()) {
    insert_results(
        w.prepared("relation_members_of_relations")(sel_relations).exec(),
        sel_relations);
  }
}

bool readonly_pgsql_selection::supports_historical_versions() {
  return true;
}

int readonly_pgsql_selection::select_historical_nodes(
  const std::vector<osm_edition_t> &eds) {

  if (!eds.empty()) {
    std::vector<osm_nwr_id_t> ids;
    std::vector<osm_version_t> vers;
    ids.resize(eds.size());
    vers.resize(eds.size());
    for (const auto &ed : eds) {
      ids.emplace_back(ed.first);
      vers.emplace_back(ed.second);
    }
    return insert_results(
      w.prepared("select_historical_nodes")(ids)(vers)(m_redactions_visible).exec(),
      sel_historic_nodes);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_historical_ways(
  const std::vector<osm_edition_t> &eds) {
  if (!eds.empty()) {
    std::vector<osm_nwr_id_t> ids;
    std::vector<osm_version_t> vers;
    ids.resize(eds.size());
    vers.resize(eds.size());
    for (const auto &ed : eds) {
      ids.emplace_back(ed.first);
      vers.emplace_back(ed.second);
    }
    return insert_results(
      w.prepared("select_historical_ways")(ids)(vers)(m_redactions_visible).exec(),
      sel_historic_ways);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_historical_relations(
  const std::vector<osm_edition_t> &eds) {
  if (!eds.empty()) {
    std::vector<osm_nwr_id_t> ids;
    std::vector<osm_version_t> vers;
    ids.resize(eds.size());
    vers.resize(eds.size());
    for (const auto &ed : eds) {
      ids.emplace_back(ed.first);
      vers.emplace_back(ed.second);
    }
    return insert_results(
      w.prepared("select_historical_relations")(ids)(vers)(m_redactions_visible).exec(),
      sel_historic_relations);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_nodes_with_history(
  const std::vector<osm_nwr_id_t> &ids) {
  if (!ids.empty()) {
    return insert_results(
      w.prepared("select_nodes_history")(ids)(m_redactions_visible).exec(),
      sel_historic_nodes);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_ways_with_history(
  const std::vector<osm_nwr_id_t> &ids) {
  if (!ids.empty()) {
    return insert_results(
      w.prepared("select_ways_history")(ids)(m_redactions_visible).exec(),
      sel_historic_ways);
  } else {
    return 0;
  }
}

int readonly_pgsql_selection::select_relations_with_history(
  const std::vector<osm_nwr_id_t> &ids) {
  if (!ids.empty()) {
    return insert_results(w.prepared("select_relations_history")
      (ids)(m_redactions_visible).exec(),
      sel_historic_relations);
  } else {
    return 0;
  }
}

void readonly_pgsql_selection::set_redactions_visible(bool visible) {
  m_redactions_visible = visible;
}

int readonly_pgsql_selection::select_historical_by_changesets(
  const std::vector<osm_changeset_id_t> &ids) {

  int selected = 0;

  if (!ids.empty()) {
    selected += insert_results(w.prepared("select_nodes_by_changesets")
      (ids)(m_redactions_visible).exec(),
      sel_historic_nodes);
    selected += insert_results(w.prepared("select_ways_by_changesets")
      (ids)(m_redactions_visible).exec(),
      sel_historic_ways);
    selected += insert_results(w.prepared("select_relations_by_changesets")
      (ids)(m_redactions_visible).exec(),
      sel_historic_relations);
  }

  return selected;
}

bool readonly_pgsql_selection::supports_changesets() {
  return true;
}

int readonly_pgsql_selection::select_changesets(const std::vector<osm_changeset_id_t> &ids) {
  if (!ids.empty()) {
    return insert_results(w.prepared("select_changesets")(ids).exec(), sel_changesets);
  } else {
    return 0;
  }
}

void readonly_pgsql_selection::select_changeset_discussions() {
  include_changeset_discussions = true;
}

readonly_pgsql_selection::factory::factory(const po::variables_map &opts)
    : m_connection(connect_db_str(opts)),
      m_cache_connection(connect_db_str(opts)),
#if PQXX_VERSION_MAJOR >= 4
      m_errorhandler(m_connection),
      m_cache_errorhandler(m_cache_connection),
#endif
      m_cache_tx(m_cache_connection, "changeset_cache"),
      m_cache(boost::bind(fetch_changeset, boost::ref(m_cache_tx), _1),
              opts["cachesize"].as<size_t>()) {

  if (m_connection.server_version() < 90300) {
    throw std::runtime_error("Expected Postgres version 9.3+, currently installed version "
        + std::to_string(m_connection.server_version()));
  }

  // set the connections to use the appropriate charset.
  m_connection.set_client_encoding(opts["charset"].as<std::string>());
  m_cache_connection.set_client_encoding(opts["charset"].as<std::string>());

  // set the connection to use readonly transaction.
  m_connection.set_variable("default_transaction_read_only", "true");

  // ignore notice messages
#if PQXX_VERSION_MAJOR < 4
  m_connection.set_noticer(
      std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));
  m_cache_connection.set_noticer(
      std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));
#endif

  logger::message("Preparing prepared statements.");

  // clang-format off

  // select nodes with bbox
  m_connection.prepare("visible_node_in_bbox",
    "SELECT id "
      "FROM current_nodes "
      "WHERE tile = ANY($1) "
        "AND latitude BETWEEN $2 AND $3 "
        "AND longitude BETWEEN $4 AND $5 "
        "AND visible = true "
      "LIMIT $6")
    PREPARE_ARGS(("bigint[]")("integer")("integer")("integer")("integer")("integer"));

  // selecting node, way and relation visibility information
  m_connection.prepare("visible_node",
    "SELECT visible FROM current_nodes WHERE id = $1")PREPARE_ARGS(("bigint"));
  m_connection.prepare("visible_way",
    "SELECT visible FROM current_ways WHERE id = $1")PREPARE_ARGS(("bigint"));
  m_connection.prepare("visible_relation",
    "SELECT visible FROM current_relations WHERE id = $1")PREPARE_ARGS(("bigint"));

  // selecting a set of objects as a list
  m_connection.prepare("select_nodes",
    "SELECT id "
      "FROM current_nodes "
      "WHERE id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("select_ways",
    "SELECT id "
      "FROM current_ways "
      "WHERE id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("select_relations",
    "SELECT id "
      "FROM current_relations "
      "WHERE id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("select_changesets",
    "SELECT id "
      "FROM changesets "
      "WHERE id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));

  // select ways used by nodes
  m_connection.prepare("ways_from_nodes",
    "SELECT DISTINCT wn.way_id AS id "
      "FROM current_way_nodes wn "
      "WHERE wn.node_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  // select nodes used by ways
  m_connection.prepare("nodes_from_ways",
    "SELECT DISTINCT wn.node_id AS id "
      "FROM current_way_nodes wn "
      "WHERE wn.way_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));

  // Queries for getting relation parents of objects
  m_connection.prepare("relation_parents_of_nodes",
    "SELECT DISTINCT rm.relation_id AS id "
      "FROM current_relation_members rm "
      "WHERE rm.member_type = 'Node' "
        "AND rm.member_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("relation_parents_of_ways",
    "SELECT DISTINCT rm.relation_id AS id "
      "FROM current_relation_members rm "
      "WHERE rm.member_type = 'Way' "
        "AND rm.member_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("relation_parents_of_relations",
    "SELECT DISTINCT rm.relation_id AS id "
      "FROM current_relation_members rm "
      "WHERE rm.member_type = 'Relation' "
        "AND rm.member_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));

  // queries for filling elements which are used as members in relations
  m_connection.prepare("nodes_from_relations",
    "SELECT DISTINCT rm.member_id AS id "
      "FROM current_relation_members rm "
      "WHERE rm.member_type = 'Node' "
        "AND rm.relation_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("ways_from_relations",
    "SELECT DISTINCT rm.member_id AS id "
      "FROM current_relation_members rm "
      "WHERE rm.member_type = 'Way' "
        "AND rm.relation_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("relation_members_of_relations",
    "SELECT DISTINCT rm.member_id AS id "
      "FROM current_relation_members rm "
      "WHERE rm.member_type = 'Relation' "
        "AND rm.relation_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));

  m_connection.prepare("select_nodes_history",
    "SELECT node_id AS id, version "
      "FROM nodes "
      "WHERE node_id = ANY($1) AND "
            "(redaction_id IS NULL OR $2 = TRUE)")
    PREPARE_ARGS(("bigint[]")("boolean"));
  m_connection.prepare("select_ways_history",
    "SELECT way_id AS id, version "
      "FROM ways "
      "WHERE way_id = ANY($1) AND "
            "(redaction_id IS NULL OR $2 = TRUE)")
    PREPARE_ARGS(("bigint[]")("boolean"));
  m_connection.prepare("select_relations_history",
    "SELECT relation_id AS id, version "
      "FROM relations "
      "WHERE relation_id = ANY($1) AND "
            "(redaction_id IS NULL OR $2 = TRUE)")
    PREPARE_ARGS(("bigint[]")("boolean"));

  m_connection.prepare("select_historical_nodes",
    "WITH wanted(id, version) AS ("
      "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
    ")"
    "SELECT n.node_id AS id, n.version "
      "FROM nodes n "
      "INNER JOIN wanted w ON n.node_id = w.id AND n.version = w.version "
      "WHERE (n.redaction_id IS NULL OR $3 = TRUE)")
    PREPARE_ARGS(("bigint[]")("bigint[]")("boolean"));
  m_connection.prepare("select_historical_ways",
    "WITH wanted(id, version) AS ("
      "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
    ")"
    "SELECT w.way_id AS id, w.version "
      "FROM ways w "
      "INNER JOIN wanted x ON w.way_id = x.id AND w.version = x.version "
      "WHERE (w.redaction_id IS NULL OR $3 = TRUE)")
    PREPARE_ARGS(("bigint[]")("bigint[]")("boolean"));
  m_connection.prepare("select_historical_relations",
    "WITH wanted(id, version) AS ("
      "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
    ")"
    "SELECT r.relation_id AS id, r.version "
      "FROM relations r "
      "INNER JOIN wanted x ON r.relation_id = x.id AND r.version = x.version "
      "WHERE (r.redaction_id IS NULL OR $3 = TRUE)")
    PREPARE_ARGS(("bigint[]")("bigint[]")("boolean"));

  // ------------------------- NODE EXTRACTION -------------------------------
  m_connection.prepare("extract_nodes",
    "SELECT n.id, n.latitude, n.longitude, n.visible, "
        "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "n.changeset_id, n.version, array_agg(t.k) as tag_k, array_agg(t.v) as tag_v "
      "FROM current_nodes n "
        "LEFT JOIN current_node_tags t ON n.id=t.node_id "
      "WHERE n.id = ANY($1) "
      "GROUP BY n.id ORDER BY n.id")
    PREPARE_ARGS(("bigint[]"));

  m_connection.prepare("extract_historic_nodes",
    "WITH wanted(id, version) AS ("
      "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
    ")"
    "SELECT n.node_id AS id, n.latitude, n.longitude, n.visible, "
        "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "n.changeset_id, n.version, array_agg(t.k) as tag_k, array_agg(t.v) as tag_v "
      "FROM nodes n "
        "INNER JOIN wanted x ON n.node_id = x.id AND n.version = x.version "
        "LEFT JOIN node_tags t ON n.node_id = t.node_id AND n.version = t.version "
      "GROUP BY n.node_id, n.version ORDER BY n.node_id, n.version")
    PREPARE_ARGS(("bigint[]")("bigint[]"));

  // -------------------------- WAY EXTRACTION -------------------------------
  m_connection.prepare("extract_ways",
    "SELECT w.id, w.visible, "
        "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "w.changeset_id, w.version, t.keys as tag_k, t.values as tag_v, "
        "wn.node_ids as node_ids "
      "FROM current_ways w "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) as keys, array_agg(v) as values "
           "FROM current_way_tags WHERE w.id=way_id) t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(node_id) as node_ids "
           "FROM "
             "(SELECT node_id FROM current_way_nodes WHERE w.id=way_id "
              "ORDER BY sequence_id) x) wn ON true "
      "WHERE w.id = ANY($1) "
      "ORDER BY w.id")
    PREPARE_ARGS(("bigint[]"));

  m_connection.prepare("extract_historic_ways",
    "WITH wanted(id, version) AS ("
      "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
    ")"
    "SELECT w.way_id AS id, w.visible, "
        "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "w.changeset_id, w.version, t.keys as tag_k, t.values as tag_v, "
        "wn.node_ids as node_ids "
      "FROM ways w "
        "INNER JOIN wanted x ON w.way_id = x.id AND w.version = x.version "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) as keys, array_agg(v) as values "
           "FROM way_tags WHERE w.way_id=way_id AND w.version=version) t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(node_id) as node_ids "
           "FROM "
             "(SELECT node_id FROM way_nodes "
              "WHERE w.way_id=way_id AND w.version=version "
              "ORDER BY sequence_id) x) wn ON true "
      "ORDER BY w.way_id, w.version")
    PREPARE_ARGS(("bigint[]")("bigint[]"));

  // --------------------- RELATION EXTRACTION -------------------------------
  m_connection.prepare("extract_relations",
    "SELECT r.id, r.visible, "
        "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "r.changeset_id, r.version, t.keys as tag_k, t.values as tag_v, "
        "rm.types as member_types, rm.ids as member_ids, rm.roles as member_roles "
      "FROM current_relations r "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) as keys, array_agg(v) as values "
           "FROM current_relation_tags WHERE r.id=relation_id) t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(member_type) as types, "
           "array_agg(member_role) as roles, array_agg(member_id) as ids "
           "FROM "
             "(SELECT * FROM current_relation_members WHERE r.id=relation_id "
              "ORDER BY sequence_id) x) rm ON true "
      "WHERE r.id = ANY($1) "
      "ORDER BY r.id")
    PREPARE_ARGS(("bigint[]"));

  m_connection.prepare("extract_historic_relations",
    "WITH wanted(id, version) AS ("
      "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
    ")"
    "SELECT r.relation_id AS id, r.visible, "
        "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "r.changeset_id, r.version, t.keys as tag_k, t.values as tag_v, "
        "rm.types as member_types, rm.ids as member_ids, rm.roles as member_roles "
      "FROM relations r "
        "INNER JOIN wanted x ON r.relation_id = x.id AND r.version = x.version "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) as keys, array_agg(v) as values "
           "FROM relation_tags WHERE r.relation_id=relation_id AND r.version=version) t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(member_type) as types, "
           "array_agg(member_role) as roles, array_agg(member_id) as ids "
           "FROM "
             "(SELECT * FROM relation_members WHERE r.relation_id=relation_id AND r.version=version "
              "ORDER BY sequence_id) x) rm ON true "
      "ORDER BY r.relation_id, r.version")
    PREPARE_ARGS(("bigint[]")("bigint[]"));

  // --------------------------- CHANGESET EXTRACTION -----------------------
  m_connection.prepare("extract_changesets",
     "SELECT c.id, "
       "to_char(c.created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
       "to_char(c.closed_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS closed_at, "
       "c.min_lat, c.max_lat, c.min_lon, c.max_lon, c.num_changes, t.keys as tag_k, "
       "t.values as tag_v, cc.author_id as comment_author_id, "
       "cc.display_name as comment_display_name, "
       "cc.body as comment_body, cc.created_at as comment_created_at "
     "FROM changesets c "
      "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) AS keys, array_agg(v) AS values "
          "FROM changeset_tags WHERE c.id=changeset_id ) t ON true "
      "LEFT JOIN LATERAL "
        "(SELECT array_agg(author_id) as author_id, array_agg(display_name) "
        "as display_name, array_agg(body) as body, "
        "array_agg(created_at) as created_at FROM "
          "(SELECT cc.author_id, u.display_name, cc.body, "
          "to_char(cc.created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
          "FROM changeset_comments cc JOIN users u ON cc.author_id = u.id "
          "where cc.changeset_id=c.id AND cc.visible ORDER BY cc.created_at) x "
        ")cc ON true "
     "WHERE c.id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));

  // ------------------- CHANGESET DOWNLOAD QUERIES -----------------------
  m_connection.prepare("select_nodes_by_changesets",
      "SELECT n.node_id AS id, n.version "
      "FROM nodes n "
      "WHERE n.changeset_id = ANY($1) "
        "AND (n.redaction_id IS NULL OR $2 = TRUE)"
    "")
    PREPARE_ARGS(("bigint[]")("boolean"));

  m_connection.prepare("select_ways_by_changesets",
      "SELECT w.way_id AS id, w.version "
      "FROM ways w "
      "WHERE w.changeset_id = ANY($1) "
        "AND (w.redaction_id IS NULL OR $2 = TRUE)"
    "")
    PREPARE_ARGS(("bigint[]")("boolean"));

  m_connection.prepare("select_relations_by_changesets",
      "SELECT r.relation_id AS id, r.version "
      "FROM relations r "
      "WHERE r.changeset_id = ANY($1) "
        "AND (r.redaction_id IS NULL OR $2 = TRUE)"
    "")
    PREPARE_ARGS(("bigint[]")("boolean"));

  // clang-format on
}

readonly_pgsql_selection::factory::~factory() {}

boost::shared_ptr<data_selection>
readonly_pgsql_selection::factory::make_selection() {
  return boost::make_shared<readonly_pgsql_selection>(boost::ref(m_connection),
                                                      boost::ref(m_cache));
}
