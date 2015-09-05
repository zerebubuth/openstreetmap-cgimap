#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
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

#if PQXX_VERSION_MAJOR >= 4
#define PREPARE_ARGS(args)
#else
#define PREPARE_ARGS(args) args
#endif

#define MAX_ELEMENTS (50000)

namespace po = boost::program_options;
namespace pt = boost::posix_time;
using std::set;
using std::stringstream;
using std::list;
using std::vector;
using boost::shared_ptr;

// number of nodes to chunk together
#define STRIDE (1000)

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
template <> struct string_traits<set<osm_nwr_id_t> > {
  static const char *name() { return "set<osm_nwr_id_t>"; }
  static bool has_null() { return false; }
  static bool is_null(const set<osm_nwr_id_t> &) { return false; }
  static stringstream null() {
    internal::throw_null_conversion(name());
    // No, dear compiler, we don't need a return here.
    throw 0;
  }
  static void from_string(const char[], set<osm_nwr_id_t> &) {}
  static std::string to_string(const set<osm_nwr_id_t> &ids) {
    stringstream ostr;
    ostr << "{";
    std::copy(ids.begin(), ids.end(),
              infix_ostream_iterator<osm_nwr_id_t>(ostr, ","));
    ostr << "}";
    return ostr.str();
  }
};

// need this for PQXX to serialise lists of tile_id_t, which is different
// from osm_nwr_id_t
template <> struct string_traits<vector<tile_id_t> > {
  static const char *name() { return "vector<tile_id_t>"; }
  static bool has_null() { return false; }
  static bool is_null(const vector<tile_id_t> &) { return false; }
  static stringstream null() {
    internal::throw_null_conversion(name());
    // No, dear compiler, we don't need a return here.
    throw 0;
  }
  static void from_string(const char[], vector<tile_id_t> &) {}
  static std::string to_string(const vector<tile_id_t> &ids) {
    stringstream ostr;
    ostr << "{";
    std::copy(ids.begin(), ids.end(),
              infix_ostream_iterator<tile_id_t>(ostr, ","));
    ostr << "}";
    return ostr.str();
  }
};

// and again for osm_changeset_id_t
template <> struct string_traits<vector<osm_changeset_id_t> > {
  static const char *name() { return "vector<osm_changeset_id_t>"; }
  static bool has_null() { return false; }
  static bool is_null(const vector<osm_changeset_id_t> &) { return false; }
  static stringstream null() {
    internal::throw_null_conversion(name());
    // No, dear compiler, we don't need a return here.
    throw 0;
  }
  static void from_string(const char[], vector<osm_changeset_id_t> &) {}
  static std::string to_string(const vector<osm_changeset_id_t> &ids) {
    stringstream ostr;
    ostr << "{";
    std::copy(ids.begin(), ids.end(),
              infix_ostream_iterator<osm_changeset_id_t>(ostr, ","));
    ostr << "}";
    return ostr.str();
  }
};
template <> struct string_traits<set<osm_changeset_id_t> > {
  static const char *name() { return "set<osm_changeset_id_t>"; }
  static bool has_null() { return false; }
  static bool is_null(const set<osm_changeset_id_t> &) { return false; }
  static stringstream null() {
    internal::throw_null_conversion(name());
    // No, dear compiler, we don't need a return here.
    throw 0;
  }
  static void from_string(const char[], set<osm_changeset_id_t> &) {}
  static std::string to_string(const set<osm_changeset_id_t> &ids) {
    stringstream ostr;
    ostr << "{";
    std::copy(ids.begin(), ids.end(),
              infix_ostream_iterator<osm_changeset_id_t>(ostr, ","));
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

template <typename T>
inline int insert_results(const pqxx::result &res, set<T> &elems) {
  int num_inserted = 0;

  for (pqxx::result::const_iterator itr = res.begin(); itr != res.end();
       ++itr) {
    const T id = (*itr)["id"].as<T>();

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

void extract_elem(const pqxx::result::tuple &row, element_info &elem,
                  cache<osm_changeset_id_t, changeset> &changeset_cache) {
  elem.id = row["id"].as<osm_nwr_id_t>();
  elem.version = row["version"].as<int>();
  elem.timestamp = row["timestamp"].c_str();
  elem.changeset = row["changeset_id"].as<osm_changeset_id_t>();
  elem.visible = row["visible"].as<bool>();
  shared_ptr<changeset const> cs = changeset_cache.get(elem.changeset);
  if (cs->data_public) {
    elem.uid = cs->user_id;
    elem.display_name = cs->display_name;
  } else {
    elem.uid = boost::none;
    elem.display_name = boost::none;
  }
}

template <typename T>
boost::optional<T> extract_optional(const pqxx::result::field &f) {
  if (f.is_null()) {
    return boost::none;
  } else {
    return f.as<T>();
  }
}

void extract_changeset(const pqxx::result::tuple &row,
                       changeset_info &elem,
                       cache<osm_changeset_id_t, changeset> &changeset_cache) {
  elem.id = row["id"].as<osm_changeset_id_t>();
  elem.created_at = row["created_at"].c_str();
  elem.closed_at = row["closed_at"].c_str();

  shared_ptr<changeset const> cs = changeset_cache.get(elem.id);
  if (cs->data_public) {
    elem.uid = cs->user_id;
    elem.display_name = cs->display_name;
  } else {
    elem.uid = boost::none;
    elem.display_name = boost::none;
  }

  boost::optional<int64_t> min_lat = extract_optional<int64_t>(row["min_lat"]);
  boost::optional<int64_t> max_lat = extract_optional<int64_t>(row["max_lat"]);
  boost::optional<int64_t> min_lon = extract_optional<int64_t>(row["min_lon"]);
  boost::optional<int64_t> max_lon = extract_optional<int64_t>(row["max_lon"]);

  if (bool(min_lat) && bool(min_lon) && bool(max_lat) && bool(max_lon)) {
    elem.bounding_box = bbox(double(*min_lat) / SCALE,
                             double(*min_lon) / SCALE,
                             double(*max_lat) / SCALE,
                             double(*max_lon) / SCALE);
  } else {
    elem.bounding_box = boost::none;
  }

  elem.num_changes = row["num_changes"].as<size_t>();
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
  element_type type;

  switch (name[0]) {
  case 'N':
  case 'n':
    type = element_type_node;
    break;

  case 'W':
  case 'w':
    type = element_type_way;
    break;

  case 'R':
  case 'r':
    type = element_type_relation;
    break;

  default:
    // in case the name match isn't exhaustive...
    throw std::runtime_error(
        "Unexpected name not matched to type in type_from_name().");
  }

  return type;
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

readonly_pgsql_selection::readonly_pgsql_selection(
    pqxx::connection &conn, cache<osm_changeset_id_t, changeset> &changeset_cache)
    : w(conn), cc(changeset_cache)
#ifdef ENABLE_EXPERIMENTAL
    , include_changeset_discussions(false)
#endif /* ENABLE_EXPERIMENTAL */
{}

readonly_pgsql_selection::~readonly_pgsql_selection() {}

void readonly_pgsql_selection::write_nodes(output_formatter &formatter) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  logger::message("Fetching nodes");
  element_info elem;
  double lon, lat;
  tags_t tags;

  // fetch in chunks...
  set<osm_nwr_id_t>::iterator prev_itr = sel_nodes.begin();
  size_t chunk_i = 0;
  for (set<osm_nwr_id_t>::iterator n_itr = sel_nodes.begin();; ++n_itr, ++chunk_i) {
    bool at_end = n_itr == sel_nodes.end();
    if ((chunk_i >= STRIDE) || ((chunk_i > 0) && at_end)) {
      stringstream query;
      query << "select n.id, n.latitude, n.longitude, n.visible, "
               "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
               "timestamp, n.changeset_id, n.version from current_nodes n "
               "where n.id in (";
      std::copy(prev_itr, n_itr, infix_ostream_iterator<osm_nwr_id_t>(query, ","));
      query << ")";
      pqxx::result nodes = w.exec(query);

      for (pqxx::result::const_iterator itr = nodes.begin(); itr != nodes.end();
           ++itr) {
        extract_elem(*itr, elem, cc);
        lon = double((*itr)["longitude"].as<int64_t>()) / (SCALE);
        lat = double((*itr)["latitude"].as<int64_t>()) / (SCALE);
        extract_tags(w.prepared("extract_node_tags")(elem.id).exec(), tags);
        formatter.write_node(elem, lon, lat, tags);
      }

      chunk_i = 0;
      prev_itr = n_itr;
    }

    if (at_end)
      break;
  }
}

void readonly_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");
  element_info elem;
  nodes_t nodes;
  tags_t tags;

  // fetch in chunks...
  set<osm_nwr_id_t>::iterator prev_itr = sel_ways.begin();
  size_t chunk_i = 0;
  for (set<osm_nwr_id_t>::iterator n_itr = sel_ways.begin();; ++n_itr, ++chunk_i) {
    bool at_end = n_itr == sel_ways.end();
    if ((chunk_i >= STRIDE) || ((chunk_i > 0) && at_end)) {
      stringstream query;
      query << "select w.id, w.visible, w.version, w.changeset_id, "
               "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
               "timestamp from current_ways w where w.id in (";
      std::copy(prev_itr, n_itr, infix_ostream_iterator<osm_nwr_id_t>(query, ","));
      query << ")";
      pqxx::result ways = w.exec(query);

      for (pqxx::result::const_iterator itr = ways.begin(); itr != ways.end();
           ++itr) {
        extract_elem(*itr, elem, cc);
        extract_nodes(w.prepared("extract_way_nds")(elem.id).exec(), nodes);
        extract_tags(w.prepared("extract_way_tags")(elem.id).exec(), tags);
        formatter.write_way(elem, nodes, tags);
      }

      chunk_i = 0;
      prev_itr = n_itr;
    }

    if (at_end)
      break;
  }
}

void readonly_pgsql_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");
  element_info elem;
  members_t members;
  tags_t tags;

  // fetch in chunks...
  set<osm_nwr_id_t>::iterator prev_itr = sel_relations.begin();
  size_t chunk_i = 0;
  for (set<osm_nwr_id_t>::iterator n_itr = sel_relations.begin();;
       ++n_itr, ++chunk_i) {
    bool at_end = n_itr == sel_relations.end();
    if ((chunk_i >= STRIDE) || ((chunk_i > 0) && at_end)) {
      stringstream query;
      query << "select r.id, r.visible, r.version, r.changeset_id, "
               "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
               "timestamp from current_relations r where r.id in (";
      std::copy(prev_itr, n_itr, infix_ostream_iterator<osm_nwr_id_t>(query, ","));
      query << ")";
      pqxx::result relations = w.exec(query);

      for (pqxx::result::const_iterator itr = relations.begin();
           itr != relations.end(); ++itr) {
        extract_elem(*itr, elem, cc);
        extract_members(w.prepared("extract_relation_members")(elem.id).exec(),
                        members);
        extract_tags(w.prepared("extract_relation_tags")(elem.id).exec(), tags);
        formatter.write_relation(elem, members, tags);
      }

      chunk_i = 0;
      prev_itr = n_itr;
    }

    if (at_end)
      break;
  }
}

#ifdef ENABLE_EXPERIMENTAL
void readonly_pgsql_selection::write_changesets(output_formatter &formatter,
                                                const pt::ptime &now) {
  changeset_info elem;
  tags_t tags;
  comments_t comments;

  // fetch in chunks...
  set<osm_changeset_id_t>::iterator prev_itr = sel_changesets.begin();
  size_t chunk_i = 0;
  for (set<osm_changeset_id_t>::iterator n_itr = sel_changesets.begin();;
       ++n_itr, ++chunk_i) {
    bool at_end = n_itr == sel_changesets.end();
    if ((chunk_i >= STRIDE) || ((chunk_i > 0) && at_end)) {
      stringstream query;
      query << "SELECT id, "
            << "to_char(created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
            << "to_char(closed_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS closed_at, "
            << "min_lat, max_lat, min_lon, max_lon, "
            << "num_changes "
            << "FROM changesets WHERE id IN (";
      std::copy(prev_itr, n_itr, infix_ostream_iterator<osm_changeset_id_t>(query, ","));
      query << ")";

      pqxx::result changesets = w.exec(query);
      for (pqxx::result::const_iterator itr = changesets.begin();
           itr != changesets.end(); ++itr) {
        extract_changeset(*itr, elem, cc);
        extract_tags(w.prepared("extract_changeset_tags")(elem.id).exec(), tags);
        // extract_comments(w.prepared("extract_changeset_comments")(elem.id).exec(), comments);
        formatter.write_changeset(elem, tags, include_changeset_discussions, comments, now);
      }

      chunk_i = 0;
      prev_itr = n_itr;
    }

    if (at_end)
      break;
  }
}
#endif /* ENABLE_EXPERIMENTAL */

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

#ifdef ENABLE_EXPERIMENTAL
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
#endif /* ENABLE_EXPERIMENTAL */

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

  // extraction functions for child information
  m_connection.prepare("extract_way_nds",
    "SELECT node_id "
      "FROM current_way_nodes "
      "WHERE way_id=$1 "
      "ORDER BY sequence_id ASC")
    PREPARE_ARGS(("bigint"));
  m_connection.prepare("extract_relation_members",
    "SELECT member_type, member_id, member_role "
      "FROM current_relation_members "
      "WHERE relation_id=$1 "
      "ORDER BY sequence_id ASC")
    PREPARE_ARGS(("bigint"));

  // extraction functions for tags
  m_connection.prepare("extract_node_tags",
    "SELECT k, v FROM current_node_tags WHERE node_id=$1")PREPARE_ARGS(("bigint"));
  m_connection.prepare("extract_way_tags",
    "SELECT k, v FROM current_way_tags WHERE way_id=$1")PREPARE_ARGS(("bigint"));
  m_connection.prepare("extract_relation_tags",
    "SELECT k, v FROM current_relation_tags WHERE relation_id=$1")PREPARE_ARGS(("bigint"));
  m_connection.prepare("extract_changeset_tags",
    "SELECT k, v FROM changeset_tags WHERE changeset_id=$1")PREPARE_ARGS(("bigint"));

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

  // clang-format on
}

readonly_pgsql_selection::factory::~factory() {}

boost::shared_ptr<data_selection>
readonly_pgsql_selection::factory::make_selection() {
  return boost::make_shared<readonly_pgsql_selection>(boost::ref(m_connection),
                                                      boost::ref(m_cache));
}
