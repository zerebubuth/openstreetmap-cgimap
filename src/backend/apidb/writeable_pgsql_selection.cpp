#include "backend/apidb/writeable_pgsql_selection.hpp"
#include "backend/apidb/apidb.hpp"
#include "logger.hpp"
#include "backend/apidb/quad_tile.hpp"
#include "infix_ostream_iterator.hpp"

#include <set>
#include <sstream>
#include <list>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

namespace po = boost::program_options;
using std::set;
using std::stringstream;
using std::list;
using boost::shared_ptr;

namespace pqxx {
template<> struct string_traits<list<osm_id_t> >
{
  static const char *name() { return "list<osm_id_t>"; }
  static bool has_null() { return false; }
  static bool is_null(const list<osm_id_t> &) { return false; }
  static stringstream null()
  {
    internal::throw_null_conversion(name());
    // No, dear compiler, we don't need a return here.
    throw 0;
  }
  static void from_string(const char Str[], list<osm_id_t> &Obj) {
  }
  static std::string to_string(const list<osm_id_t> &ids) {
    stringstream ostr;
    ostr << "{";
    std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(ostr, ","));
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
check_table_visibility(pqxx::work &w, osm_id_t id, const std::string &prepared_name) {
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

void extract_elem(const pqxx::result::tuple &row, element_info &elem, cache<osm_id_t, changeset> &changeset_cache) {
  elem.id = row["id"].as<osm_id_t>();
  elem.version = row["version"].as<int>();
  elem.timestamp = row["timestamp"].c_str();
  elem.changeset = row["changeset_id"].as<osm_id_t>();
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

void extract_tags(const pqxx::result &res, tags_t &tags) {
  tags.clear();
  for (pqxx::result::const_iterator itr = res.begin();
       itr != res.end(); ++itr) {
    tags.push_back(std::make_pair(std::string((*itr)["k"].c_str()),
                                  std::string((*itr)["v"].c_str())));
  }
}

void extract_nodes(const pqxx::result &res, nodes_t &nodes) {
  nodes.clear();
  for (pqxx::result::const_iterator itr = res.begin();
       itr != res.end(); ++itr) {
    nodes.push_back((*itr)[0].as<osm_id_t>());
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
    throw std::runtime_error("Unexpected name not matched to type in type_from_name().");
  }

  return type;
}

void extract_members(const pqxx::result &res, members_t &members) {
  member_info member;
  members.clear();
  for (pqxx::result::const_iterator itr = res.begin();
       itr != res.end(); ++itr) {
    member.type = type_from_name((*itr)["member_type"].c_str());
    member.ref = (*itr)["member_id"].as<osm_id_t>();
    member.role = (*itr)["member_role"].c_str();
    members.push_back(member);
  }
}

} // anonymous namespace

writeable_pgsql_selection::writeable_pgsql_selection(pqxx::connection &conn, cache<osm_id_t, changeset> &changeset_cache)
  : w(conn), cc(changeset_cache) {
  w.exec("CREATE TEMPORARY TABLE tmp_nodes (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_ways (id bigint PRIMARY KEY)");
  w.exec("CREATE TEMPORARY TABLE tmp_relations (id bigint PRIMARY KEY)");
  m_tables_empty = true;
}

writeable_pgsql_selection::~writeable_pgsql_selection() {
}

void 
writeable_pgsql_selection::write_nodes(output_formatter &formatter) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  logger::message("Fetching nodes");
  element_info elem;
  double lon, lat;
  tags_t tags;

  formatter.start_element_type(element_type_node);
  pqxx::result nodes = w.prepared("extract_nodes").exec();
  for (pqxx::result::const_iterator itr = nodes.begin(); 
       itr != nodes.end(); ++itr) {
    extract_elem(*itr, elem, cc);
    lon = double((*itr)["longitude"].as<int64_t>()) / (SCALE);
    lat = double((*itr)["latitude"].as<int64_t>()) / (SCALE);
    extract_tags(w.prepared("extract_node_tags")(elem.id).exec(), tags);
    formatter.write_node(elem, lon, lat, tags);
  }
  formatter.end_element_type(element_type_node);
}

void 
writeable_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.
  logger::message("Fetching ways");
  element_info elem;
  nodes_t nodes;
  tags_t tags;

  formatter.start_element_type(element_type_way);
  pqxx::result ways = w.prepared("extract_ways").exec();
  for (pqxx::result::const_iterator itr = ways.begin(); 
       itr != ways.end(); ++itr) {
    extract_elem(*itr, elem, cc);
    extract_nodes(w.prepared("extract_way_nds")(elem.id).exec(), nodes);
    extract_tags(w.prepared("extract_way_tags")(elem.id).exec(), tags);
    formatter.write_way(elem, nodes, tags);
  }
  formatter.end_element_type(element_type_way);
}

void 
writeable_pgsql_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");
  element_info elem;
  members_t members;
  tags_t tags;

  formatter.start_element_type(element_type_relation);
  pqxx::result relations = w.prepared("extract_relations").exec();
  for (pqxx::result::const_iterator itr = relations.begin(); 
       itr != relations.end(); ++itr) {
    extract_elem(*itr, elem, cc);
    extract_members(w.prepared("extract_relation_members")(elem.id).exec(), members);
    extract_tags(w.prepared("extract_relation_tags")(elem.id).exec(), tags);
    formatter.write_relation(elem, members, tags);
  }
  formatter.end_element_type(element_type_relation);
}

data_selection::visibility_t 
writeable_pgsql_selection::check_node_visibility(osm_id_t id) {
  return check_table_visibility(w, id, "visible_node");
}

data_selection::visibility_t 
writeable_pgsql_selection::check_way_visibility(osm_id_t id) {
  return check_table_visibility(w, id, "visible_way");
}

data_selection::visibility_t 
writeable_pgsql_selection::check_relation_visibility(osm_id_t id) {
  return check_table_visibility(w, id, "visible_relation");
}

int
writeable_pgsql_selection::select_nodes(const std::list<osm_id_t> &ids) {
  m_tables_empty = false;
  return w.prepared("add_nodes_list")(ids).exec().affected_rows();
}

int
writeable_pgsql_selection::select_ways(const std::list<osm_id_t> &ids) {
  m_tables_empty = false;
  return w.prepared("add_ways_list")(ids).exec().affected_rows();
}

int
writeable_pgsql_selection::select_relations(const std::list<osm_id_t> &ids) {
  m_tables_empty = false;
  return w.prepared("add_relations_list")(ids).exec().affected_rows();
}

int
writeable_pgsql_selection::select_nodes_from_bbox(const bbox &bounds, int max_nodes) {
  const list<osm_id_t> tiles =
    tiles_for_area(bounds.minlat, bounds.minlon, 
                   bounds.maxlat, bounds.maxlon);
   
  // hack around problem with postgres' statistics, which was 
  // making it do seq scans all the time on smaug...
  w.exec("set enable_mergejoin=false");
  w.exec("set enable_hashjoin=false");

  logger::message("Filling tmp_nodes from bbox");
  // optimise for the case where this is the first query run.
  // apparently it's significantly faster without the check.
  if (!m_tables_empty) {
    throw std::runtime_error("Filling tmp_nodes, but some content already present. "
                             "This violates a design assumption, and is a bug. "
                             "Please report this to "
                             "https://github.com/zerebubuth/openstreetmap-cgimap/issues");
  }
  m_tables_empty = false;

  return w.prepared("visible_node_in_bbox")
    (tiles)
    (int(bounds.minlat * SCALE))(int(bounds.maxlat * SCALE))
    (int(bounds.minlon * SCALE))(int(bounds.maxlon * SCALE))
    (max_nodes + 1).exec().affected_rows();
}

void 
writeable_pgsql_selection::select_nodes_from_relations() {
  logger::message("Filling tmp_nodes (from relations)");

  w.prepared("nodes_from_relations").exec();
}

void 
writeable_pgsql_selection::select_ways_from_nodes() {
  logger::message("Filling tmp_ways (from nodes)");
  w.prepared("ways_from_nodes").exec();
}

void 
writeable_pgsql_selection::select_ways_from_relations() {
  logger::message("Filling tmp_ways (from relations)");
  w.prepared("ways_from_relations").exec();
}

void 
writeable_pgsql_selection::select_relations_from_ways() {
  logger::message("Filling tmp_relations (from ways)");
  w.prepared("relations_from_ways").exec();
}

void 
writeable_pgsql_selection::select_nodes_from_way_nodes() {
  w.prepared("nodes_from_way_nodes").exec();
}

void 
writeable_pgsql_selection::select_relations_from_nodes() {
  w.prepared("relations_from_nodes").exec();
}

void 
writeable_pgsql_selection::select_relations_from_relations() {
  w.prepared("relations_from_relations").exec();
}

void 
writeable_pgsql_selection::select_relations_members_of_relations() {
  w.prepared("relation_members_of_relations").exec();
}

writeable_pgsql_selection::factory::factory(const po::variables_map &opts)
  : m_connection(connect_db_str(opts)),
    m_cache_connection(connect_db_str(opts)),
    m_cache_tx(m_cache_connection, "changeset_cache"),
    m_cache(boost::bind(fetch_changeset, boost::ref(m_cache_tx), _1), opts["cachesize"].as<size_t>()) {

  // set the connections to use the appropriate charset.
  m_connection.set_client_encoding(opts["charset"].as<std::string>());
  m_cache_connection.set_client_encoding(opts["charset"].as<std::string>());

  // ignore notice messages
  m_connection.set_noticer(std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));
  m_cache_connection.set_noticer(std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));

  logger::message("Preparing prepared statements.");

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
    ("bigint[]")("integer")("integer")("integer")("integer")("integer");

  // selecting node, way and relation visibility information
  m_connection.prepare("visible_node",
    "SELECT visible FROM current_nodes WHERE id = $1")("bigint");
  m_connection.prepare("visible_way",
    "SELECT visible FROM current_ways WHERE id = $1")("bigint");
  m_connection.prepare("visible_relation",
    "SELECT visible FROM current_relations WHERE id = $1")("bigint");

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

  // extraction functions for child information
  m_connection.prepare("extract_way_nds",
    "SELECT node_id "
      "FROM current_way_nodes "
      "WHERE way_id=$1 "
      "ORDER BY sequence_id ASC")
    ("bigint");
  m_connection.prepare("extract_relation_members",
    "SELECT member_type, member_id, member_role "
      "FROM current_relation_members "
      "WHERE relation_id=$1 "
      "ORDER BY sequence_id ASC")
    ("bigint");

  // extraction functions for tags
  m_connection.prepare("extract_node_tags",
    "SELECT k, v FROM current_node_tags WHERE node_id=$1")("bigint");
  m_connection.prepare("extract_way_tags",
    "SELECT k, v FROM current_way_tags WHERE way_id=$1")("bigint");
  m_connection.prepare("extract_relation_tags",
    "SELECT k, v FROM current_relation_tags WHERE relation_id=$1")("bigint");

  // selecting a set of nodes as a list
  m_connection.prepare("add_nodes_list",
    "INSERT INTO tmp_nodes "
      "SELECT n.id AS id "
        "FROM current_nodes n "
          "LEFT JOIN tmp_nodes tn ON n.id = tn.id "
        "WHERE n.id = ANY($1) "
          "AND tn.id IS NULL")
    ("bigint[]");
  m_connection.prepare("add_ways_list",
    "INSERT INTO tmp_ways "
      "SELECT w.id AS id "
        "FROM current_ways w "
          "LEFT JOIN tmp_ways tw ON w.id = tw.id "
        "WHERE w.id = ANY($1) "
          "AND tw.id IS NULL")
    ("bigint[]");
  m_connection.prepare("add_relations_list",
    "INSERT INTO tmp_relations "
      "SELECT r.id AS id "
        "FROM current_relations r "
          "LEFT JOIN tmp_relations tr ON r.id = tr.id "
        "WHERE r.id = ANY($1) "
          "AND tr.id IS NULL")
    ("bigint[]");

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
}

writeable_pgsql_selection::factory::~factory() {
}

boost::shared_ptr<data_selection> writeable_pgsql_selection::factory::make_selection() {
  return boost::make_shared<writeable_pgsql_selection>(boost::ref(m_connection),boost::ref(m_cache));
}
