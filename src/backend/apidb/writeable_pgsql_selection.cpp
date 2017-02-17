#include "cgimap/backend/apidb/writeable_pgsql_selection.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"
#include "cgimap/infix_ostream_iterator.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include <set>
#include <sstream>
#include <list>
#include <vector>
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

struct node_from_db {
  element_info elem;
  double lon, lat;
  tags_t tags;

  inline void format(output_formatter &f) {
    f.write_node(elem, lon, lat, tags);
  }

  static std::string current_tags() { return "extract_node_tags"; }
  static std::string historic_tags() { return "extract_historic_node_tags"; }
};

struct way_from_db {
  element_info elem;
  nodes_t nodes;
  tags_t tags;

  inline void format(output_formatter &f) {
    f.write_way(elem, nodes, tags);
  }

  static std::string current_tags() { return "extract_way_tags"; }
  static std::string historic_tags() { return "extract_historic_way_tags"; }
};

struct rel_from_db {
  element_info elem;
  members_t members;
  tags_t tags;

  inline void format(output_formatter &f) {
    f.write_relation(elem, members, tags);
  }

  static std::string current_tags() { return "extract_relation_tags"; }
  static std::string historic_tags() { return "extract_historic_relation_tags"; }
};

template <typename T>
void extract_extra(const pqxx::result::tuple &r, T& t, pqxx::work &w, bool historic);

template <>
void extract_extra<node_from_db>(const pqxx::result::tuple &r, node_from_db &n,
                                 pqxx::work &w, bool historic) {
  n.lon = double(r["longitude"].as<int64_t>()) / (SCALE);
  n.lat = double(r["latitude"].as<int64_t>()) / (SCALE);
}

template <>
void extract_extra<way_from_db>(const pqxx::result::tuple &r, way_from_db &way,
                                 pqxx::work &w, bool historic) {
  if (historic) {
    extract_nodes(w.prepared("extract_historic_way_nds")(way.elem.id)(way.elem.version).exec(), way.nodes);
  } else {
    extract_nodes(w.prepared("extract_way_nds")(way.elem.id).exec(), way.nodes);
  }
}

template <>
void extract_extra<rel_from_db>(const pqxx::result::tuple &r, rel_from_db &rel,
                                pqxx::work &w, bool historic) {
  if (historic) {
    extract_members(w.prepared("extract_historic_relation_members")(rel.elem.id)(rel.elem.version).exec(), rel.members);
  } else {
    extract_members(w.prepared("extract_relation_members")(rel.elem.id).exec(), rel.members);
  }
}

template <typename T>
struct unpack_elems {
  typedef cache<osm_changeset_id_t, changeset> changeset_cache;
  changeset_cache &m_cc;
  pqxx::work &m_w;
  bool m_historic;

  unpack_elems(changeset_cache &cc, pqxx::work &w, bool historic)
    : m_cc(cc), m_w(w), m_historic(historic) {}

  T operator()(const pqxx::result::tuple &r) const {
    T t;
    extract_elem(r, t.elem, m_cc);
    extract_extra<T>(r, t, m_w, m_historic);
    if (m_historic) {
      extract_tags(m_w.prepared(T::historic_tags())(t.elem.id)(t.elem.version).exec(), t.tags);
    } else {
      extract_tags(m_w.prepared(T::current_tags())(t.elem.id).exec(), t.tags);
    }
    return t;
  }
};

template <typename T>
void unpack_format(pqxx::work &w, cache<osm_changeset_id_t, changeset> &cc,
                   const std::string &statement, bool historic,
                   output_formatter &formatter) {
  typedef unpack_elems<T> unpacker;
  typedef boost::transform_iterator<unpacker, pqxx::result::const_iterator> iterator;

  pqxx::result res = w.prepared(statement).exec();

  unpacker func(cc, w, historic);
  const iterator end(res.end(), func);
  for (iterator itr(res.begin(), func); itr != end; ++itr) {
    itr->format(formatter);
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

} // anonymous namespace

writeable_pgsql_selection::writeable_pgsql_selection(
    pqxx::connection &conn, cache<osm_changeset_id_t, changeset> &changeset_cache)
    : w(conn), cc(changeset_cache)
    , include_changeset_discussions(false)
    , m_redactions_visible(false) {
  w.exec("CREATE TEMPORARY TABLE tmp_nodes (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_ways (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_relations (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_changesets (id bigint PRIMARY KEY)");
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
    unpack_format<node_from_db>(w, cc, "extract_nodes", false, formatter);
  }

  if (!m_historic_tables_empty) {
    if (!m_tables_empty) {
      w.prepared("drop_current_node_versions_from_historic").exec();
    }

    unpack_format<node_from_db>(w, cc, "extract_historic_nodes", true, formatter);
  }
}

void writeable_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");

  if (!m_tables_empty) {
    unpack_format<way_from_db>(w, cc, "extract_ways", false, formatter);
  }

  if (!m_historic_tables_empty) {
    if (!m_tables_empty) {
      w.prepared("drop_current_way_versions_from_historic").exec();
    }

    unpack_format<way_from_db>(w, cc, "extract_historic_ways", true, formatter);
  }
}

void writeable_pgsql_selection::write_relations(output_formatter &formatter) {
  // grab the relations, relation members and tags
  logger::message("Fetching relations");

  if (!m_tables_empty) {
    unpack_format<rel_from_db>(w, cc, "extract_relations", false, formatter);
  }

  if (!m_historic_tables_empty) {
    if (!m_tables_empty) {
      w.prepared("drop_current_relation_versions_from_historic").exec();
    }

    unpack_format<rel_from_db>(w, cc, "extract_historic_relations", true, formatter);
  }
}

void writeable_pgsql_selection::write_changesets(output_formatter &formatter,
                                                 const pt::ptime &now) {
  changeset_info elem;
  tags_t tags;
  comments_t comments;

  pqxx::result changesets = w.prepared("extract_changesets").exec();
  for (pqxx::result::const_iterator itr = changesets.begin();
       itr != changesets.end(); ++itr) {
    extract_changeset(*itr, elem, cc);
    extract_tags(w.prepared("extract_changeset_tags")(elem.id).exec(), tags);
    extract_comments(w.prepared("extract_changeset_comments")(elem.id).exec(), comments);
    elem.comments_count = comments.size();
    formatter.write_changeset(elem, tags, include_changeset_discussions, comments, now);
  }
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
    selected += w.prepared("add_historic_relation")(ed.first)(ed.second)
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
        "n.changeset_id, n.version "
      "FROM current_nodes n "
        "JOIN tmp_nodes tn ON n.id = tn.id");
  m_connection.prepare("extract_ways",
    "SELECT w.id, w.visible, w.version, w.changeset_id, "
        "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp "
      "FROM current_ways w "
        "JOIN tmp_ways tw ON w.id=tw.id");
  m_connection.prepare("extract_relations",
     "SELECT r.id, r.visible, r.version, r.changeset_id, "
        "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp "
      "FROM current_relations r "
        "JOIN tmp_relations tr ON tr.id=r.id");
  m_connection.prepare("extract_changesets",
     "SELECT c.id, "
       "to_char(c.created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
       "to_char(c.closed_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS closed_at, "
       "c.min_lat, c.max_lat, c.min_lon, c.max_lon, "
       "c.num_changes "
     "FROM changesets c "
       "JOIN tmp_changesets tc ON tc.id=c.id");

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
        "n.changeset_id, n.version "
      "FROM nodes n "
        "JOIN tmp_historic_nodes tn "
        "ON n.node_id = tn.node_id AND n.version = tn.version");
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
    "SELECT w.way_id AS id, w.visible, "
        "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "w.changeset_id, w.version "
      "FROM ways w "
        "JOIN tmp_historic_ways tw "
        "ON w.way_id = tw.way_id AND w.version = tw.version");
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
           "relation_id = $1 AND version = $2")
    PREPARE_ARGS(("bigint")("bigint"));

  m_connection.prepare("extract_historic_relations",
    "SELECT r.relation_id AS id, r.visible, "
        "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
        "r.changeset_id, r.version "
      "FROM relations r "
        "JOIN tmp_historic_relations tr "
        "ON r.relation_id = tr.relation_id AND r.version = tr.version");
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

  // clang-format on
}

writeable_pgsql_selection::factory::~factory() {}

boost::shared_ptr<data_selection>
writeable_pgsql_selection::factory::make_selection() {
  return boost::make_shared<writeable_pgsql_selection>(boost::ref(m_connection),
                                                       boost::ref(m_cache));
}
