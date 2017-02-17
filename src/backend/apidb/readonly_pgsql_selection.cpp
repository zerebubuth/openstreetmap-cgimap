#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
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

void extract_comments(const pqxx::result &res, comments_t &comments) {
  changeset_comment_info comment;
  comments.clear();
  for (pqxx::result::const_iterator itr = res.begin(); itr != res.end();
       ++itr) {
    comment.author_id = (*itr)["author_id"].as<osm_user_id_t>();
    comment.author_display_name = (*itr)["display_name"].c_str();
    comment.body = (*itr)["body"].c_str();
    comment.created_at = (*itr)["created_at"].c_str();
    comments.push_back(comment);
  }
}

struct node_from_db {
  element_info elem;
  double lon, lat;
  tags_t tags;

  void extract(const pqxx::result::tuple &row, output_formatter &formatter,
               bool historic, cache<osm_changeset_id_t, changeset> &cc,
               pqxx::work &w) {
    extract_elem(row, elem, cc);
    lon = double(row["longitude"].as<int64_t>()) / (SCALE);
    lat = double(row["latitude"].as<int64_t>()) / (SCALE);
    if (historic) {
      extract_tags(w.prepared("extract_historic_node_tags")(elem.id)(elem.version).exec(), tags);
    } else {
      extract_tags(w.prepared("extract_node_tags")(elem.id).exec(), tags);
    }
    formatter.write_node(elem, lon, lat, tags);
  }

  static void current_query(
    stringstream &query, set<osm_nwr_id_t>::const_iterator begin,
    set<osm_nwr_id_t>::const_iterator end) {

    query << "select cur.id, cur.latitude, cur.longitude, cur.visible, "
      "to_char(cur.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
      "timestamp, cur.changeset_id, cur.version from current_nodes cur"
      " where cur.id in (";
    std::copy(begin, end, infix_ostream_iterator<osm_nwr_id_t>(query, ","));
    query << ")";
  }

  static void historic_query(
    stringstream &query, osm_nwr_id_t id, osm_version_t version) {

    query << "select hst.node_id AS id, hst.latitude, hst.longitude, hst.visible, "
      "to_char(hst.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
      "timestamp, hst.changeset_id, hst.version from nodes hst "
      "where "
      "hst.node_id = " << id << " and "
      "hst.version = " << version;
  }
};

struct way_from_db {
  element_info elem;
  nodes_t nodes;
  tags_t tags;

  void extract(const pqxx::result::tuple &row, output_formatter &formatter,
               bool historic, cache<osm_changeset_id_t, changeset> &cc,
               pqxx::work &w) {
    extract_elem(row, elem, cc);
    if (historic) {
      extract_nodes(w.prepared("extract_historic_way_nds")(elem.id)(elem.version).exec(), nodes);
      extract_tags(w.prepared("extract_historic_way_tags")(elem.id)(elem.version).exec(), tags);
    } else {
      extract_nodes(w.prepared("extract_way_nds")(elem.id).exec(), nodes);
      extract_tags(w.prepared("extract_way_tags")(elem.id).exec(), tags);
    }
    formatter.write_way(elem, nodes, tags);
  }

  static void current_query(
    stringstream &query, set<osm_nwr_id_t>::const_iterator begin,
    set<osm_nwr_id_t>::const_iterator end) {

    query << "select w.id, w.visible, w.version, w.changeset_id, "
      "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
      "timestamp from current_ways w where w.id in (";
    std::copy(begin, end, infix_ostream_iterator<osm_nwr_id_t>(query, ","));
    query << ")";
  }

  static void historic_query(
    stringstream &query, osm_nwr_id_t id, osm_version_t version) {

    query << "select hst.way_id AS id, hst.visible, "
      "to_char(hst.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
      "timestamp, hst.changeset_id, hst.version from ways hst "
      "where "
      "hst.way_id = " << id << " and "
      "hst.version = " << version;
  }
};

struct rel_from_db {
  element_info elem;
  members_t members;
  tags_t tags;

  void extract(const pqxx::result::tuple &row, output_formatter &formatter,
               bool historic, cache<osm_changeset_id_t, changeset> &cc,
               pqxx::work &w) {
    extract_elem(row, elem, cc);
    if (historic) {
      extract_members(w.prepared("extract_historic_relation_members")(elem.id)(elem.version).exec(), members);
      extract_tags(w.prepared("extract_historic_relation_tags")(elem.id)(elem.version).exec(), tags);
    } else {
      extract_members(w.prepared("extract_relation_members")(elem.id).exec(), members);
      extract_tags(w.prepared("extract_relation_tags")(elem.id).exec(), tags);
    }
    formatter.write_relation(elem, members, tags);
  }

  static void current_query(
    stringstream &query, set<osm_nwr_id_t>::const_iterator begin,
    set<osm_nwr_id_t>::const_iterator end) {

    query << "select r.id, r.visible, r.version, r.changeset_id, "
      "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
      "timestamp from current_relations r where r.id in (";
    std::copy(begin, end, infix_ostream_iterator<osm_nwr_id_t>(query, ","));
    query << ")";
  }

  static void historic_query(
    stringstream &query, osm_nwr_id_t id, osm_version_t version) {

    query << "select hst.relation_id AS id, hst.visible, "
      "to_char(hst.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as "
      "timestamp, hst.changeset_id, hst.version from relations hst "
      "where "
      "hst.relation_id = " << id << " and "
      "hst.version = " << version;
  }
};

template <typename T>
void output_chunk(pqxx::result &res, std::set<osm_edition_t> &versions,
                  output_formatter &formatter, bool historic,
                  cache<osm_changeset_id_t, changeset> &cc, pqxx::work &w) {
  T t;

  for (pqxx::result::const_iterator itr = res.begin();
       itr != res.end(); ++itr) {
    t.extract(*itr, formatter, historic, cc, w);
    versions.erase(std::make_pair(t.elem.id, t.elem.version));
  }
}

template <typename T>
void fetch_current_in_chunks(
  const std::set<osm_nwr_id_t> &sel,
  std::set<osm_edition_t> &editions,
  output_formatter &formatter,
  cache<osm_changeset_id_t, changeset> &cc, pqxx::work &w) {

  // fetch in chunks...
  set<osm_nwr_id_t>::const_iterator prev_itr = sel.begin();
  size_t chunk_i = 0;
  for (set<osm_nwr_id_t>::const_iterator n_itr = sel.begin();; ++n_itr, ++chunk_i) {
    bool at_end = n_itr == sel.end();
    if ((chunk_i >= STRIDE) || ((chunk_i > 0) && at_end)) {
      stringstream query;
      T::current_query(query, prev_itr, n_itr);
      pqxx::result res = w.exec(query);

      output_chunk<T>(res, editions, formatter, false, cc, w);

      chunk_i = 0;
      prev_itr = n_itr;
    }

    if (at_end)
      break;
  }
}

template <typename T>
void fetch_historic(
  const std::set<osm_edition_t> &sel, output_formatter &formatter,
  cache<osm_changeset_id_t, changeset> &cc, pqxx::work &w) {
  std::set<osm_edition_t> empty;

  for (set<osm_edition_t>::const_iterator n_itr = sel.begin();
       n_itr != sel.end(); ++n_itr) {

    stringstream query;
    T::historic_query(query, n_itr->first, n_itr->second);
    pqxx::result res = w.exec(query);

    output_chunk<T>(res, empty, formatter, true, cc, w);
  }
}

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
  std::set<osm_edition_t> historic_nodes = sel_historic_nodes;
  fetch_current_in_chunks<node_from_db>(sel_nodes, historic_nodes, formatter, cc, w);
  fetch_historic<node_from_db>(historic_nodes, formatter, cc, w);
}

void readonly_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");

  std::set<osm_edition_t> historic_ways = sel_historic_ways;
  fetch_current_in_chunks<way_from_db>(sel_ways, historic_ways, formatter, cc, w);
  fetch_historic<way_from_db>(historic_ways, formatter, cc, w);
}

void readonly_pgsql_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");

  std::set<osm_edition_t> historic_relations = sel_historic_relations;
  fetch_current_in_chunks<rel_from_db>(sel_relations, historic_relations, formatter, cc, w);
  fetch_historic<rel_from_db>(historic_relations, formatter, cc, w);
}

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
        extract_comments(w.prepared("extract_changeset_comments")(elem.id).exec(), comments);
        elem.comments_count = comments.size();
        formatter.write_changeset(elem, tags, include_changeset_discussions, comments, now);
      }

      chunk_i = 0;
      prev_itr = n_itr;
    }

    if (at_end)
      break;
  }
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
  int num_inserted = 0;

  for (std::vector<osm_edition_t>::const_iterator itr = eds.begin();
       itr != eds.end(); ++itr) {
    const osm_edition_t ed = *itr;

    // note: only count the *new* rows inserted.
    if (sel_historic_ways.insert(ed).second) {
      ++num_inserted;
    }
  }

  return num_inserted;
}

int readonly_pgsql_selection::select_historical_relations(
  const std::vector<osm_edition_t> &eds) {
  int num_inserted = 0;

  for (std::vector<osm_edition_t>::const_iterator itr = eds.begin();
       itr != eds.end(); ++itr) {
    const osm_edition_t ed = *itr;

    // note: only count the *new* rows inserted.
    if (sel_historic_relations.insert(ed).second) {
      ++num_inserted;
    }
  }

  return num_inserted;
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
    return insert_results(w.prepared("select_relations_history")(ids).exec(),
                          sel_historic_relations);
  } else {
    return 0;
  }
}

void readonly_pgsql_selection::set_redactions_visible(bool visible) {
  m_redactions_visible = visible;
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
  m_connection.prepare("extract_changeset_comments",
    "SELECT cc.author_id, u.display_name, cc.body, "
        "to_char(cc.created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
      "FROM changeset_comments cc "
      "JOIN users u ON cc.author_id = u.id "
      "WHERE cc.changeset_id=$1 AND cc.visible "
      "ORDER BY cc.created_at ASC")
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

  m_connection.prepare("extract_historic_node_tags",
    "SELECT k, v FROM node_tags WHERE node_id=$1 AND version=$2")
    PREPARE_ARGS(("bigint")("bigint"));
  m_connection.prepare("extract_historic_way_tags",
    "SELECT k, v FROM way_tags WHERE way_id=$1 AND version=$2")
    PREPARE_ARGS(("bigint")("bigint"));
  m_connection.prepare("extract_historic_relation_tags",
    "SELECT k, v FROM relation_tags WHERE relation_id=$1 AND version=$2")
    PREPARE_ARGS(("bigint")("bigint"));

  m_connection.prepare("extract_historic_way_nds",
    "SELECT node_id "
      "FROM way_nodes "
      "WHERE way_id=$1 AND version=$2 "
      "ORDER BY sequence_id ASC")
    PREPARE_ARGS(("bigint")("bigint"));
  m_connection.prepare("extract_historic_relation_members",
    "SELECT member_type, member_id, member_role "
      "FROM relation_members "
      "WHERE relation_id=$1 AND version=$2 "
      "ORDER BY sequence_id ASC")
    PREPARE_ARGS(("bigint")("bigint"));

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
      "WHERE relation_id = ANY($1)")
    PREPARE_ARGS(("bigint[]"));

  m_connection.prepare("select_historical_nodes",
    "WITH wanted(id, version) AS ("
      "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
    ")"
    "SELECT n.node_id AS id, n.version "
      "FROM nodes n "
      "INNER JOIN wanted w ON n.node_id = w.id AND n.version = w.version "
      "WHERE (n.redaction_id IS NULL OR $3 = TRUE)")
    PREPARE_ARGS(("bigint[]")("bigint[]")("boolean"));

  // clang-format on
}

readonly_pgsql_selection::factory::~factory() {}

boost::shared_ptr<data_selection>
readonly_pgsql_selection::factory::make_selection() {
  return boost::make_shared<readonly_pgsql_selection>(boost::ref(m_connection),
                                                      boost::ref(m_cache));
}
