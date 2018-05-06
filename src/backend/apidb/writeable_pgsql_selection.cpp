#include "cgimap/backend/apidb/writeable_pgsql_selection.hpp"
#include "cgimap/backend/apidb/common_pgsql_selection.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"
#include "cgimap/infix_ostream_iterator.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
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

} // anonymous namespace

writeable_pgsql_selection::writeable_pgsql_selection(
    pqxx::connection &conn, cache<osm_changeset_id_t, changeset> &changeset_cache)
    : w(conn), cc(changeset_cache)
    , include_changeset_discussions(false)
    , m_redactions_visible(false) {
  w.set_variable("default_transaction_read_only", "false");
  w.exec("CREATE TEMPORARY TABLE tmp_nodes (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_ways (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_relations (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_changesets (id bigint PRIMARY KEY)");
  w.set_variable("default_transaction_read_only", "true");
  m_tables_empty = true;

  w.exec("CREATE TEMPORARY TABLE tmp_historic_nodes "
         "(node_id bigint, version bigint, PRIMARY KEY (node_id, version))");
  w.exec("CREATE TEMPORARY TABLE tmp_historic_ways "
         "(way_id bigint, version bigint, PRIMARY KEY (way_id, version))");
  w.exec("CREATE TEMPORARY TABLE tmp_historic_relations "
         "(relation_id bigint, version bigint, PRIMARY KEY (relation_id, version))");
  m_historic_tables_empty = true;
}

writeable_pgsql_selection::~writeable_pgsql_selection() {}

void writeable_pgsql_selection::write_nodes(output_formatter &formatter) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  logger::message("Fetching nodes");

  if (!m_tables_empty) {
    extract_nodes(w.prepared("extract_nodes").exec(), formatter, cc);
  }

  if (!m_historic_tables_empty) {
    if (!m_tables_empty) {
      w.prepared("drop_current_node_versions_from_historic").exec();
    }
    extract_nodes(w.prepared("extract_historic_nodes").exec(), formatter, cc);
  }
}

void writeable_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");

  if (!m_tables_empty) {
    extract_ways(w.prepared("extract_ways").exec(), formatter, cc);
  }

  if (!m_historic_tables_empty) {
    if (!m_tables_empty) {
      w.prepared("drop_current_way_versions_from_historic").exec();
    }
    extract_ways(w.prepared("extract_historic_ways").exec(), formatter, cc);
  }
}

void writeable_pgsql_selection::write_relations(output_formatter &formatter) {
  // grab the relations, relation members and tags
  logger::message("Fetching relations");
  if (!m_tables_empty) {
    extract_relations(w.prepared("extract_relations").exec(), formatter, cc);
  }

  if (!m_historic_tables_empty) {
    if (!m_tables_empty) {
      w.prepared("drop_current_relation_versions_from_historic").exec();
    }
    extract_relations(w.prepared("extract_historic_relations").exec(), formatter, cc);
  }
}

void writeable_pgsql_selection::write_changesets(output_formatter &formatter,
                                                 const pt::ptime &now) {
  pqxx::result changesets = w.prepared("extract_changesets").exec();
  extract_changesets(changesets, formatter, cc, now, include_changeset_discussions);
}

data_selection::visibility_t
writeable_pgsql_selection::check_node_visibility(osm_nwr_id_t id) {
  return check_table_visibility(w, id, "visible_node");
}

data_selection::visibility_t
writeable_pgsql_selection::check_way_visibility(osm_nwr_id_t id) {
  return check_table_visibility(w, id, "visible_way");
}

data_selection::visibility_t
writeable_pgsql_selection::check_relation_visibility(osm_nwr_id_t id) {
  return check_table_visibility(w, id, "visible_relation");
}

int writeable_pgsql_selection::select_nodes(const std::vector<osm_nwr_id_t> &ids) {
  m_tables_empty = false;
  return w.prepared("add_nodes_list")(ids).exec().affected_rows();
}

int writeable_pgsql_selection::select_ways(const std::vector<osm_nwr_id_t> &ids) {
  m_tables_empty = false;
  return w.prepared("add_ways_list")(ids).exec().affected_rows();
}

int writeable_pgsql_selection::select_relations(
    const std::vector<osm_nwr_id_t> &ids) {
  m_tables_empty = false;
  return w.prepared("add_relations_list")(ids).exec().affected_rows();
}

int writeable_pgsql_selection::select_nodes_from_bbox(const bbox &bounds,
                                                      int max_nodes) {
  const vector<tile_id_t> tiles = tiles_for_area(bounds.minlat, bounds.minlon,
                                                 bounds.maxlat, bounds.maxlon);

  // hack around problem with postgres' statistics, which was
  // making it do seq scans all the time on smaug...
  w.exec("set enable_mergejoin=false");
  w.exec("set enable_hashjoin=false");

  logger::message("Filling tmp_nodes from bbox");
  // optimise for the case where this is the first query run.
  // apparently it's significantly faster without the check.
  if (!m_tables_empty) {
    throw std::runtime_error(
        "Filling tmp_nodes, but some content already present. "
        "This violates a design assumption, and is a bug. "
        "Please report this to "
        "https://github.com/zerebubuth/openstreetmap-cgimap/issues");
  }
  m_tables_empty = false;

  return w.prepared("visible_node_in_bbox")(tiles)(int(bounds.minlat * SCALE))(
               int(bounds.maxlat * SCALE))(int(bounds.minlon * SCALE))(
               int(bounds.maxlon * SCALE))(max_nodes + 1)
      .exec()
      .affected_rows();
}

void writeable_pgsql_selection::select_nodes_from_relations() {
  logger::message("Filling tmp_nodes (from relations)");

  w.prepared("nodes_from_relations").exec();
}

void writeable_pgsql_selection::select_ways_from_nodes() {
  logger::message("Filling tmp_ways (from nodes)");
  w.prepared("ways_from_nodes").exec();
}

void writeable_pgsql_selection::select_ways_from_relations() {
  logger::message("Filling tmp_ways (from relations)");
  w.prepared("ways_from_relations").exec();
}

void writeable_pgsql_selection::select_relations_from_ways() {
  logger::message("Filling tmp_relations (from ways)");
  w.prepared("relations_from_ways").exec();
}

void writeable_pgsql_selection::select_nodes_from_way_nodes() {
  w.prepared("nodes_from_way_nodes").exec();
}

void writeable_pgsql_selection::select_relations_from_nodes() {
  w.prepared("relations_from_nodes").exec();
}

void writeable_pgsql_selection::select_relations_from_relations() {
  w.prepared("relations_from_relations").exec();
}

void writeable_pgsql_selection::select_relations_members_of_relations() {
  w.prepared("relation_members_of_relations").exec();
}

bool writeable_pgsql_selection::supports_historical_versions() {
  return true;
}

int writeable_pgsql_selection::select_historical_nodes(
  const std::vector<osm_edition_t> &eds) {
  m_historic_tables_empty = false;

  size_t selected = 0;
  BOOST_FOREACH(osm_edition_t ed, eds) {
    selected += w.prepared("add_historic_node")
      (ed.first)(ed.second)(m_redactions_visible)
      .exec().affected_rows();
  }

  assert(selected < size_t(std::numeric_limits<int>::max()));
  return selected;
}

int writeable_pgsql_selection::select_historical_ways(
  const std::vector<osm_edition_t> &eds) {
  m_historic_tables_empty = false;

  size_t selected = 0;
  BOOST_FOREACH(osm_edition_t ed, eds) {
    selected += w.prepared("add_historic_way")
      (ed.first)(ed.second)(m_redactions_visible)
      .exec().affected_rows();
  }

  assert(selected < size_t(std::numeric_limits<int>::max()));
  return selected;
}

int writeable_pgsql_selection::select_historical_relations(
  const std::vector<osm_edition_t> &eds) {
  m_historic_tables_empty = false;

  size_t selected = 0;
  BOOST_FOREACH(osm_edition_t ed, eds) {
    selected += w.prepared("add_historic_relation")
      (ed.first)(ed.second)(m_redactions_visible)
      .exec().affected_rows();
  }

  assert(selected < size_t(std::numeric_limits<int>::max()));
  return selected;
}

int writeable_pgsql_selection::select_nodes_with_history(
  const std::vector<osm_nwr_id_t> &ids) {
  m_historic_tables_empty = false;
  size_t selected = 0;
  BOOST_FOREACH(osm_nwr_id_t id, ids) {
    selected += w.prepared("add_all_versions_of_node")
      (id)(m_redactions_visible)
      .exec().affected_rows();
  }

  assert(selected < size_t(std::numeric_limits<int>::max()));
  return selected;
}

int writeable_pgsql_selection::select_ways_with_history(
  const std::vector<osm_nwr_id_t> &ids) {
  m_historic_tables_empty = false;
  size_t selected = 0;
  BOOST_FOREACH(osm_nwr_id_t id, ids) {
    selected += w.prepared("add_all_versions_of_way")
      (id)(m_redactions_visible)
      .exec().affected_rows();
  }

  assert(selected < size_t(std::numeric_limits<int>::max()));
  return selected;
}

int writeable_pgsql_selection::select_relations_with_history(
  const std::vector<osm_nwr_id_t> &ids) {
  m_historic_tables_empty = false;
  size_t selected = 0;
  BOOST_FOREACH(osm_nwr_id_t id, ids) {
    selected += w.prepared("add_all_versions_of_relation")
      (id)(m_redactions_visible)
      .exec().affected_rows();
  }

  assert(selected < size_t(std::numeric_limits<int>::max()));
  return selected;
}

void writeable_pgsql_selection::set_redactions_visible(bool visible) {
  m_redactions_visible = visible;
}

int writeable_pgsql_selection::select_historical_by_changesets(
  const std::vector<osm_changeset_id_t> &ids) {

  int selected = 0;

  if (!ids.empty()) {
    selected += w.prepared("add_nodes_by_changesets")
      (ids)(m_redactions_visible)
      .exec().affected_rows();
    selected += w.prepared("add_ways_by_changesets")
      (ids)(m_redactions_visible)
      .exec().affected_rows();
    selected += w.prepared("add_relations_by_changesets")
      (ids)(m_redactions_visible)
      .exec().affected_rows();
  }

  if (selected > 0) {
    m_historic_tables_empty = false;
  }

  return selected;
}

bool writeable_pgsql_selection::supports_changesets() {
  return true;
}

int writeable_pgsql_selection::select_changesets(const std::vector<osm_changeset_id_t> &ids) {
  return w.prepared("add_changesets_list")(ids).exec().affected_rows();
}

void writeable_pgsql_selection::select_changeset_discussions() {
  include_changeset_discussions = true;
}

namespace {
/* this exists solely because converting boost::any seems to just
 * do type equality, with no fall-back to boost::lexical_cast or
 * convertible types. from the documentation, it looks like it
 * really ought to work, but several hours of debugging later and
 * i can't seem to figure out how. instead, it's easily possible
 * to just try a bunch of conversions and see if any of them
 * work.
 */
size_t get_or_convert_cachesize(const po::variables_map &opts) {
  const boost::any &val = opts["cachesize"].value();

  {
    const size_t *v = boost::any_cast<size_t>(&val);
    if (v) {
      return *v;
    }
  }

  {
    const int *v = boost::any_cast<int>(&val);
    if (v) {
      return *v;
    }
  }

  {
    const std::string *v = boost::any_cast<std::string>(&val);
    if (v) {
      return boost::lexical_cast<size_t>(*v);
    }
  }

  throw std::runtime_error("Unable to convert cachesize option to size_t.");
}
} // anonymous namespace

writeable_pgsql_selection::factory::factory(const po::variables_map &opts)
    : m_connection(connect_db_str(opts)),
      m_cache_connection(connect_db_str(opts)),
#if PQXX_VERSION_MAJOR >= 4
      m_errorhandler(m_connection),
      m_cache_errorhandler(m_cache_connection),
#endif
      m_cache_tx(m_cache_connection, "changeset_cache"),
      m_cache(boost::bind(fetch_changeset, boost::ref(m_cache_tx), _1),
              get_or_convert_cachesize(opts)) {

  check_postgres_version(m_connection);

  // set the connections to use the appropriate charset.
  m_connection.set_client_encoding(opts["charset"].as<std::string>());
  m_cache_connection.set_client_encoding(opts["charset"].as<std::string>());

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
  // version which ignores any existing tmp_nodes IDs
  //
  // note that we make the assumption that when this is executed
  // there are no existing nodes in the tmp_nodes table. there is
  // a check to ensure this assumption is not violated
  // (m_tables_empty).
  m_connection.prepare("visible_node_in_bbox",
    "INSERT INTO tmp_nodes "
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

  // extraction functions for getting the data back out when the
  // selection set has been built up.
  m_connection.prepare("extract_nodes",
    "SELECT n.id, n.latitude, n.longitude, n.visible, "
        "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "n.changeset_id, n.version, array_agg(t.k) as tag_k, array_agg(t.v) as tag_v "
      "FROM current_nodes n "
        "JOIN tmp_nodes tn ON n.id=tn.id "
        "LEFT JOIN current_node_tags t ON n.id=t.node_id GROUP BY n.id");
  m_connection.prepare("extract_ways",
    "SELECT w.id, w.visible, w.version, w.changeset_id, "
        "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "t.keys as tag_k, t.values as tag_v, wn.node_ids as node_ids "
      "FROM current_ways w JOIN tmp_ways tw ON w.id=tw.id "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) AS keys, array_agg(v) AS values "
          "FROM current_way_tags WHERE w.id=way_id ) t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(node_id) AS node_ids from "
            "(SELECT * FROM current_way_nodes WHERE w.id=way_id "
             "ORDER BY sequence_id) x "
          ") wn ON true ");
  m_connection.prepare("extract_relations",
     "SELECT r.id, r.visible, r.version, r.changeset_id, "
        "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "t.keys as tag_k, t.values as tag_v, rm.types as member_types, "
        "rm.ids as member_ids, rm.roles as member_roles "
      "FROM current_relations r JOIN tmp_relations tr ON tr.id=r.id "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) AS keys, array_agg(v) AS values "
          "FROM current_relation_tags WHERE r.id=relation_id ) t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(member_type) AS types, array_agg(member_id) AS ids, "
          "array_agg(member_role) AS roles FROM "
            "( SELECT * FROM current_relation_members WHERE r.id=relation_id "
            "ORDER BY sequence_id) x"
          ")rm ON true");
  m_connection.prepare("extract_changesets",
     "SELECT c.id, "
       "to_char(c.created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
       "to_char(c.closed_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS closed_at, "
       "c.min_lat, c.max_lat, c.min_lon, c.max_lon, c.num_changes, t.keys as tag_k, "
       "t.values as tag_v, cc.author_id as comment_author_id, "
       "cc.display_name as comment_display_name, "
       "cc.body as comment_body, cc.created_at as comment_created_at "
     "FROM changesets c JOIN tmp_changesets tc ON tc.id=c.id "
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
        ")cc ON true");

  // selecting a set of nodes as a list
  m_connection.prepare("add_nodes_list",
    "INSERT INTO tmp_nodes "
      "SELECT n.id AS id "
        "FROM current_nodes n "
          "LEFT JOIN tmp_nodes tn ON n.id = tn.id "
        "WHERE n.id = ANY($1) "
          "AND tn.id IS NULL")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("add_ways_list",
    "INSERT INTO tmp_ways "
      "SELECT w.id AS id "
        "FROM current_ways w "
          "LEFT JOIN tmp_ways tw ON w.id = tw.id "
        "WHERE w.id = ANY($1) "
          "AND tw.id IS NULL")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("add_relations_list",
    "INSERT INTO tmp_relations "
      "SELECT r.id AS id "
        "FROM current_relations r "
          "LEFT JOIN tmp_relations tr ON r.id = tr.id "
        "WHERE r.id = ANY($1) "
          "AND tr.id IS NULL")
    PREPARE_ARGS(("bigint[]"));
  m_connection.prepare("add_changesets_list",
    "INSERT INTO tmp_changesets "
      "SELECT c.id from changesets c "
        "WHERE c.id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));

  // queries for filling elements which are used as members in relations
  m_connection.prepare("nodes_from_relations",
    "INSERT INTO tmp_nodes "
      "SELECT DISTINCT rm.member_id AS id "
        "FROM tmp_relations tr "
          "JOIN current_relation_members rm ON rm.relation_id = tr.id "
          "LEFT JOIN tmp_nodes tn ON rm.member_id = tn.id "
        "WHERE rm.member_type='Node' "
          "AND tn.id IS NULL");
  m_connection.prepare("ways_from_relations",
    "INSERT INTO tmp_ways "
      "SELECT DISTINCT rm.member_id AS id "
        "FROM tmp_relations tr "
          "JOIN current_relation_members rm ON rm.relation_id = tr.id "
          "LEFT JOIN tmp_ways tw ON rm.member_id = tw.id "
        "WHERE rm.member_type='Way' "
          "AND tw.id IS NULL");
  m_connection.prepare("relation_members_of_relations",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT rm.member_id AS id "
        "FROM tmp_relations tr "
          "JOIN current_relation_members rm ON rm.relation_id = tr.id "
          "LEFT JOIN tmp_relations xr ON rm.member_id = xr.id "
        "WHERE rm.member_type='Relation' "
          "AND xr.id IS NULL");

  // select ways which use nodes already in the working set
  m_connection.prepare("ways_from_nodes",
    "INSERT INTO tmp_ways "
      "SELECT DISTINCT wn.way_id AS id "
        "FROM current_way_nodes wn "
          "JOIN tmp_nodes tn ON wn.node_id = tn.id "
          "LEFT JOIN tmp_ways tw ON wn.way_id = tw.id "
        "WHERE tw.id IS NULL");
  // select nodes used by ways already in the working set
  m_connection.prepare("nodes_from_way_nodes",
    "INSERT INTO tmp_nodes "
      "SELECT DISTINCT wn.node_id AS id "
        "FROM tmp_ways tw "
          "JOIN current_way_nodes wn ON tw.id = wn.way_id "
          "LEFT JOIN tmp_nodes tn ON wn.node_id = tn.id "
        "WHERE tn.id IS NULL");

  // selecting relations which have members which are already in
  // the working set.
  m_connection.prepare("relations_from_nodes",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT rm.relation_id "
        "FROM tmp_nodes tn "
          "JOIN current_relation_members rm "
            "ON (tn.id = rm.member_id AND rm.member_type='Node') "
          "LEFT JOIN tmp_relations tr ON rm.relation_id = tr.id "
        "WHERE tr.id IS NULL");
  m_connection.prepare("relations_from_ways",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT rm.relation_id AS id "
        "FROM tmp_ways tw "
          "JOIN current_relation_members rm "
            "ON (tw.id = rm.member_id AND rm.member_type='Way') "
          "LEFT JOIN tmp_relations tr ON rm.relation_id = tr.id "
        "WHERE tr.id IS NULL");
  m_connection.prepare("relations_from_relations",
    "INSERT INTO tmp_relations "
      "SELECT DISTINCT rm.relation_id "
        "FROM tmp_relations tr "
          "JOIN current_relation_members rm "
            "ON (tr.id = rm.member_id AND rm.member_type='Relation') "
          "LEFT JOIN tmp_relations xr ON rm.relation_id = xr.id "
        "WHERE xr.id IS NULL");

  // select a historic (id, version) edition of a node
  m_connection.prepare("add_historic_node",
    "INSERT INTO tmp_historic_nodes "
       "SELECT node_id, version "
         "FROM nodes "
         "WHERE "
           "node_id = $1 AND version = $2 AND"
           "(redaction_id IS NULL OR $3 = TRUE)")
    PREPARE_ARGS(("bigint")("bigint")("boolean"));

  m_connection.prepare("drop_current_node_versions_from_historic",
    "WITH cv AS ("
      "SELECT n.id, n.version "
        "FROM current_nodes n "
        "JOIN tmp_nodes tn ON n.id = tn.id) "
    "DELETE FROM tmp_historic_nodes thn USING cv "
      "WHERE thn.node_id = cv.id AND thn.version = cv.version");

  m_connection.prepare("extract_historic_nodes",
    "SELECT n.node_id AS id, n.latitude, n.longitude, n.visible, "
        "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "n.changeset_id, n.version, array_agg(t.k) as tag_k, array_agg(t.v) as tag_v "
      "FROM nodes n "
        "JOIN tmp_historic_nodes tn "
          "ON n.node_id = tn.node_id AND n.version = tn.version "
        "LEFT JOIN node_tags t "
          "ON n.node_id = t.node_id AND n.version = t.version "
      "GROUP BY n.node_id, n.version");
  m_connection.prepare("extract_historic_node_tags",
    "SELECT k, v FROM node_tags WHERE node_id=$1 AND version=$2")
    PREPARE_ARGS(("bigint")("bigint"));

  m_connection.prepare("drop_current_way_versions_from_historic",
    "WITH cv AS ("
      "SELECT w.id, w.version "
        "FROM current_ways w "
        "JOIN tmp_ways tw ON w.id = tw.id) "
    "DELETE FROM tmp_historic_ways thw USING cv "
      "WHERE thw.way_id = cv.id AND thw.version = cv.version");

  // select a historic (id, version) edition of a way
  m_connection.prepare("add_historic_way",
    "INSERT INTO tmp_historic_ways "
       "SELECT way_id, version "
         "FROM ways "
         "WHERE "
           "way_id = $1 AND version = $2 AND"
           "(redaction_id IS NULL OR $3 = TRUE)")
    PREPARE_ARGS(("bigint")("bigint")("boolean"));

  m_connection.prepare("extract_historic_ways",
    "SELECT w.way_id AS id, w.visible, w.version, w.changeset_id, "
        "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "t.keys as tag_k, t.values as tag_v, wn.node_ids as node_ids "
      "FROM ways w "
        "JOIN tmp_historic_ways tw "
          "ON w.way_id = tw.way_id AND w.version = tw.version "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) AS keys, array_agg(v) AS values "
          "FROM way_tags WHERE w.way_id = way_id ) t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(node_id) AS node_ids from "
            "(SELECT * FROM way_nodes "
              "WHERE w.way_id = way_id AND w.version = version "
              "ORDER BY sequence_id) x "
          ") wn ON true ");
  m_connection.prepare("extract_historic_way_tags",
    "SELECT k, v FROM way_tags WHERE way_id=$1 AND version=$2")
    PREPARE_ARGS(("bigint")("bigint"));

  m_connection.prepare("extract_historic_way_nds",
    "SELECT node_id "
      "FROM way_nodes "
      "WHERE way_id=$1 AND version=$2"
      "ORDER BY sequence_id ASC")
    PREPARE_ARGS(("bigint")("bigint"));

  m_connection.prepare("drop_current_relation_versions_from_historic",
    "WITH cv AS ("
      "SELECT r.id, r.version "
        "FROM current_relations r "
        "JOIN tmp_relations tr ON r.id = tr.id) "
    "DELETE FROM tmp_historic_relations thr USING cv "
      "WHERE thr.relation_id = cv.id AND thr.version = cv.version");

  // select a historic (id, version) edition of a relation
  m_connection.prepare("add_historic_relation",
    "INSERT INTO tmp_historic_relations "
       "SELECT relation_id, version "
         "FROM relations "
         "WHERE "
           "relation_id = $1 AND version = $2 AND"
           "(redaction_id IS NULL OR $3 = TRUE)")
    PREPARE_ARGS(("bigint")("bigint")("boolean"));

  m_connection.prepare("extract_historic_relations",
     "SELECT r.relation_id AS id, r.visible, r.version, r.changeset_id, "
        "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "t.keys as tag_k, t.values as tag_v, rm.types as member_types, "
        "rm.ids as member_ids, rm.roles as member_roles "
      "FROM relations r "
        "JOIN tmp_historic_relations tr "
          "ON tr.relation_id = r.relation_id AND tr.version = r.version "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(k) AS keys, array_agg(v) AS values "
           "FROM relation_tags "
            "WHERE r.relation_id = relation_id AND r.version = version "
          ") t ON true "
        "LEFT JOIN LATERAL "
          "(SELECT array_agg(member_type) AS types, array_agg(member_id) AS ids, "
           "array_agg(member_role) AS roles "
           "FROM "
            "(SELECT * FROM relation_members "
              "WHERE r.relation_id = relation_id AND r.version = version "
            "ORDER BY sequence_id) x"
          ") rm ON true");
  m_connection.prepare("extract_historic_relation_tags",
    "SELECT k, v FROM relation_tags WHERE relation_id=$1 AND version=$2")
    PREPARE_ARGS(("bigint")("bigint"));
  m_connection.prepare("extract_historic_relation_members",
    "SELECT member_type, member_id, member_role "
      "FROM relation_members "
      "WHERE relation_id=$1 AND version=$2 "
      "ORDER BY sequence_id ASC")
    PREPARE_ARGS(("bigint")("bigint"));

  m_connection.prepare("add_all_versions_of_node",
    "INSERT INTO tmp_historic_nodes "
      "SELECT n.node_id, n.version "
      "FROM nodes n "
      "LEFT JOIN tmp_historic_nodes t "
      "ON t.node_id = n.node_id AND t.version = n.version "
      "WHERE n.node_id = $1 AND t.node_id IS NULL AND "
            "(n.redaction_id IS NULL OR $2 = TRUE)")
    PREPARE_ARGS(("bigint")("boolean"));
  m_connection.prepare("add_all_versions_of_way",
    "INSERT INTO tmp_historic_ways "
      "SELECT w.way_id, w.version "
      "FROM ways w "
      "LEFT JOIN tmp_historic_ways t "
      "ON t.way_id = w.way_id AND t.version = w.version "
      "WHERE w.way_id = $1 AND t.way_id IS NULL AND "
            "(w.redaction_id IS NULL OR $2 = TRUE)")
    PREPARE_ARGS(("bigint")("boolean"));
  m_connection.prepare("add_all_versions_of_relation",
    "INSERT INTO tmp_historic_relations "
      "SELECT r.relation_id, r.version "
      "FROM relations r "
      "LEFT JOIN tmp_historic_relations t "
      "ON t.relation_id = r.relation_id AND t.version = r.version "
      "WHERE r.relation_id = $1 AND t.relation_id IS NULL AND "
            "(r.redaction_id IS NULL OR $2 = TRUE)")
    PREPARE_ARGS(("bigint")("boolean"));

  // ------------------- CHANGESET DOWNLOAD QUERIES -----------------------
  m_connection.prepare("add_nodes_by_changesets",
    "INSERT INTO tmp_historic_nodes "
      "SELECT n.node_id, n.version "
      "FROM nodes n "
      "LEFT JOIN tmp_historic_nodes t "
      "ON t.node_id = n.node_id AND t.version = n.version "
      "WHERE n.changeset_id = ANY($1) "
        "AND t.node_id IS NULL "
        "AND (n.redaction_id IS NULL OR $2 = TRUE)"
    "")
    PREPARE_ARGS(("bigint[]")("boolean"));

  m_connection.prepare("add_ways_by_changesets",
    "INSERT INTO tmp_historic_ways "
      "SELECT w.way_id, w.version "
      "FROM ways w "
      "LEFT JOIN tmp_historic_ways t "
      "ON t.way_id = w.way_id AND t.version = w.version "
      "WHERE w.changeset_id = ANY($1) "
        "AND t.way_id IS NULL "
        "AND (w.redaction_id IS NULL OR $2 = TRUE)"
    "")
    PREPARE_ARGS(("bigint[]")("boolean"));

  m_connection.prepare("add_relations_by_changesets",
    "INSERT INTO tmp_historic_relations "
      "SELECT r.relation_id, r.version "
      "FROM relations r "
      "LEFT JOIN tmp_historic_relations t "
      "ON t.relation_id = r.relation_id AND t.version = r.version "
      "WHERE r.changeset_id = ANY($1) "
        "AND t.relation_id IS NULL "
        "AND (r.redaction_id IS NULL OR $2 = TRUE)"
    "")
    PREPARE_ARGS(("bigint[]")("boolean"));

  // clang-format on
}

writeable_pgsql_selection::factory::~factory() {}

boost::shared_ptr<data_selection>
writeable_pgsql_selection::factory::make_selection() {
  return boost::make_shared<writeable_pgsql_selection>(boost::ref(m_connection),
                                                       boost::ref(m_cache));
}
