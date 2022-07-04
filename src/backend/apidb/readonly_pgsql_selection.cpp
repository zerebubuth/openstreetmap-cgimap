#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
#include "cgimap/backend/apidb/common_pgsql_selection.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/options.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"
#include "cgimap/infix_ostream_iterator.hpp"

#include <functional>
#include <sstream>
#include <list>
#include <vector>

#include <boost/algorithm/string/trim.hpp>


namespace po = boost::program_options;
using std::set;
using std::stringstream;
using std::list;
using std::vector;


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
check_table_visibility(Transaction_Manager  &m, osm_nwr_id_t id,
                       const std::string &prepared_name) {
  pqxx::result res = m.exec_prepared(prepared_name, id);

  if (res.empty())
    return data_selection::non_exist;

  if (res[0][0].as<bool>()) {
    return data_selection::exists;
  } else {
    return data_selection::deleted;
  }
}

using pqxx_tuple = pqxx::result::reference;

template <typename T> T id_of(const pqxx_tuple &);

template <>
osm_nwr_id_t id_of<osm_nwr_id_t>(const pqxx_tuple &row) {
  return row["id"].as<osm_nwr_id_t>();
}

template <>
osm_changeset_id_t id_of<osm_changeset_id_t>(const pqxx_tuple &row) {
  return row["id"].as<osm_changeset_id_t>();
}

template <>
osm_edition_t id_of<osm_edition_t>(const pqxx_tuple &row) {
  auto id = row["id"].as<osm_nwr_id_t>();
  auto ver = row["version"].as<osm_version_t>();
  return osm_edition_t(id, ver);
}

template <typename T>
inline int insert_results(const pqxx::result &res, set<T> &elems) {
  int num_inserted = 0;

  for (const auto & row : res) {
    const T id = id_of<T>(row);

    // note: only count the *new* rows inserted.
    if (elems.insert(id).second) {
      ++num_inserted;
    }
  }
  return num_inserted;
}

} // anonymous namespace

readonly_pgsql_selection::readonly_pgsql_selection(
    Transaction_Owner_Base& to, cache<osm_changeset_id_t, changeset> &changeset_cache)
    : m(to)
    , include_changeset_discussions(false)
    , m_redactions_visible(false)
    , cc(changeset_cache) {}

readonly_pgsql_selection::~readonly_pgsql_selection() = default;

void readonly_pgsql_selection::write_nodes(output_formatter &formatter) {
  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.

  m.prepare("extract_nodes",
     "SELECT n.id, n.latitude, n.longitude, n.visible, "
         "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
         "n.changeset_id, n.version, array_agg(t.k) as tag_k, array_agg(t.v) as tag_v "
       "FROM current_nodes n "
         "LEFT JOIN current_node_tags t ON n.id=t.node_id "
       "WHERE n.id = ANY($1) "
       "GROUP BY n.id ORDER BY n.id");


  logger::message("Fetching nodes");
  if (!sel_nodes.empty()) {
      // lambda function gets notified about each single element, allowing us to
      // remove all object versions from historic nodes, that are already
      // contained in current nodes
    extract_nodes(m.exec_prepared("extract_nodes", sel_nodes),
		  formatter,
		  [&](const element_info& elem)
                    { sel_historic_nodes.erase(osm_edition_t(elem.id, elem.version)); },
		  cc);
  }

  m.prepare("extract_historic_nodes",
     "WITH wanted(id, version) AS ("
       "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
     ")"
     "SELECT n.node_id AS id, n.latitude, n.longitude, n.visible, "
         "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS timestamp, "
         "n.changeset_id, n.version, array_agg(t.k) as tag_k, array_agg(t.v) as tag_v "
       "FROM nodes n "
         "INNER JOIN wanted x ON n.node_id = x.id AND n.version = x.version "
         "LEFT JOIN node_tags t ON n.node_id = t.node_id AND n.version = t.version "
       "GROUP BY n.node_id, n.version ORDER BY n.node_id, n.version");

  if (!sel_historic_nodes.empty()) {
    std::vector<osm_nwr_id_t> ids, versions;
    for (const auto &ed : sel_historic_nodes) {
      ids.emplace_back(ed.first);
      versions.emplace_back(ed.second);
    }
    extract_nodes(m.exec_prepared("extract_historic_nodes", ids, versions),
		  formatter,
		  [&](const element_info&) {},
		  cc);
  }
}

void readonly_pgsql_selection::write_ways(output_formatter &formatter) {
  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.

  m.prepare("extract_ways",
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
       "ORDER BY w.id");

  logger::message("Fetching ways");
  if (!sel_ways.empty()) {
      // lambda function gets notified about each single element, allowing us to
    // remove all object versions from historic ways, that are already
    // contained in current ways
    extract_ways(m.exec_prepared("extract_ways", sel_ways),
		 formatter,
		 [&](const element_info& elem)
		    { sel_historic_ways.erase(osm_edition_t(elem.id, elem.version)); },
		 cc);
  }

  m.prepare("extract_historic_ways",
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
       "ORDER BY w.way_id, w.version");

  if (!sel_historic_ways.empty()) {
    std::vector<osm_nwr_id_t> ids, versions;
    for (const auto &ed : sel_historic_ways) {
      ids.emplace_back(ed.first);
      versions.emplace_back(ed.second);
    }
    extract_ways(m.exec_prepared("extract_historic_ways", ids, versions),
		 formatter,
		 [&](const element_info&) {},
		 cc);
  }
}

void readonly_pgsql_selection::write_relations(output_formatter &formatter) {
  logger::message("Fetching relations");

  m.prepare("extract_relations",
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
       "ORDER BY r.id");


  if (!sel_relations.empty()) {
    // lambda function gets notified about each single element, allowing us to
    // remove all object versions from historic relations, that are already
    // contained in current relations
    extract_relations(m.exec_prepared("extract_relations", sel_relations),
		      formatter,
	              [&](const element_info& elem)
		        { sel_historic_relations.erase(osm_edition_t(elem.id, elem.version)); },
		      cc);
  }

  m.prepare("extract_historic_relations",
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
       "ORDER BY r.relation_id, r.version");

  if (!sel_historic_relations.empty()) {
    std::vector<osm_nwr_id_t> ids, versions;
    for (const auto &ed : sel_historic_relations) {
      ids.emplace_back(ed.first);
      versions.emplace_back(ed.second);
    }
    extract_relations(m.exec_prepared("extract_historic_relations", ids, versions),
		      formatter,
	              [&](const element_info&) {},
		      cc);
  }
}

void readonly_pgsql_selection::write_changesets(output_formatter &formatter,
                                                const std::chrono::system_clock::time_point &now) {

  m.prepare("extract_changesets",
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
      "WHERE c.id = ANY($1)");

  pqxx::result changesets = m.exec_prepared("extract_changesets", sel_changesets);
  extract_changesets(changesets, formatter, cc, now, include_changeset_discussions);
}

data_selection::visibility_t
readonly_pgsql_selection::check_node_visibility(osm_nwr_id_t id) {
  m.prepare("visible_node",
     "SELECT visible FROM current_nodes WHERE id = $1");
  return check_table_visibility(m, id, "visible_node");
}

data_selection::visibility_t
readonly_pgsql_selection::check_way_visibility(osm_nwr_id_t id) {
  m.prepare("visible_way",
     "SELECT visible FROM current_ways WHERE id = $1");
  return check_table_visibility(m, id, "visible_way");
}

data_selection::visibility_t
readonly_pgsql_selection::check_relation_visibility(osm_nwr_id_t id) {

  m.prepare("visible_relation", "SELECT visible FROM current_relations WHERE id = $1");

  return check_table_visibility(m, id, "visible_relation");
}

int readonly_pgsql_selection::select_nodes(const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return 0;

  m.prepare("select_nodes", "SELECT id FROM current_nodes WHERE id = ANY($1)");

  return insert_results(m.exec_prepared("select_nodes", ids), sel_nodes);
}

int readonly_pgsql_selection::select_ways(const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return 0;

  m.prepare("select_ways", "SELECT id FROM current_ways WHERE id = ANY($1)");

  return insert_results(m.exec_prepared("select_ways", ids), sel_ways);
}

int readonly_pgsql_selection::select_relations(const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return 0;

  m.prepare("select_relations", "SELECT id FROM current_relations WHERE id = ANY($1)");

  return insert_results(m.exec_prepared("select_relations", ids),
                        sel_relations);
}

int readonly_pgsql_selection::select_nodes_from_bbox(const bbox &bounds,
                                                     int max_nodes) {
  const std::vector<tile_id_t> tiles = tiles_for_area(
      bounds.minlat, bounds.minlon, bounds.maxlat, bounds.maxlon);

  // select nodes with bbox
 m.prepare("visible_node_in_bbox",
    "SELECT id "
      "FROM current_nodes "
      "WHERE tile = ANY($1) "
        "AND latitude BETWEEN $2 AND $3 "
        "AND longitude BETWEEN $4 AND $5 "
        "AND visible = true "
      "LIMIT $6");

  // hack around problem with postgres' statistics, which was
  // making it do seq scans all the time on smaug...
  m.exec("set enable_mergejoin=false");
  m.exec("set enable_hashjoin=false");

  return insert_results(
      m.exec_prepared("visible_node_in_bbox", tiles,
		      int(bounds.minlat * global_settings::get_scale()),
		      int(bounds.maxlat * global_settings::get_scale()),
		      int(bounds.minlon * global_settings::get_scale()),
		      int(bounds.maxlon * global_settings::get_scale()),
		      (max_nodes + 1)),
      sel_nodes);
}

void readonly_pgsql_selection::select_nodes_from_relations() {
  logger::message("Filling sel_nodes (from relations)");

  if (!sel_relations.empty()) {

    m.prepare("nodes_from_relations",
       "SELECT DISTINCT rm.member_id AS id "
	 "FROM current_relation_members rm "
	 "WHERE rm.member_type = 'Node' "
	   "AND rm.relation_id = ANY($1)");

    insert_results(m.exec_prepared("nodes_from_relations", sel_relations),
                   sel_nodes);
  }
}

void readonly_pgsql_selection::select_ways_from_nodes() {
  logger::message("Filling sel_ways (from nodes)");

  if (!sel_nodes.empty()) {
    m.prepare("ways_from_nodes",
       "SELECT DISTINCT wn.way_id AS id "
	 "FROM current_way_nodes wn "
	 "WHERE wn.node_id = ANY($1)");

    insert_results(m.exec_prepared("ways_from_nodes", sel_nodes), sel_ways);
  }
}

void readonly_pgsql_selection::select_ways_from_relations() {
  logger::message("Filling sel_ways (from relations)");

  if (!sel_relations.empty()) {
    m.prepare("ways_from_relations",
       "SELECT DISTINCT rm.member_id AS id "
	 "FROM current_relation_members rm "
	 "WHERE rm.member_type = 'Way' "
	   "AND rm.relation_id = ANY($1)");

    insert_results(m.exec_prepared("ways_from_relations", sel_relations),
                   sel_ways);
  }
}

void readonly_pgsql_selection::select_relations_from_ways() {
  logger::message("Filling sel_relations (from ways)");

  if (!sel_ways.empty()) {
    m.prepare("relation_parents_of_ways",
       "SELECT DISTINCT rm.relation_id AS id "
	 "FROM current_relation_members rm "
	 "WHERE rm.member_type = 'Way' "
	   "AND rm.member_id = ANY($1)");

    insert_results(m.exec_prepared("relation_parents_of_ways", sel_ways),
                   sel_relations);
  }
}

void readonly_pgsql_selection::select_nodes_from_way_nodes() {
  if (!sel_ways.empty()) {
    m.prepare("nodes_from_ways",
       "SELECT DISTINCT wn.node_id AS id "
	 "FROM current_way_nodes wn "
	 "WHERE wn.way_id = ANY($1)");

    insert_results(m.exec_prepared("nodes_from_ways", sel_ways), sel_nodes);
  }
}

void readonly_pgsql_selection::select_relations_from_nodes() {
  if (!sel_nodes.empty()) {
    m.prepare("relation_parents_of_nodes",
       "SELECT DISTINCT rm.relation_id AS id "
	 "FROM current_relation_members rm "
	 "WHERE rm.member_type = 'Node' "
	   "AND rm.member_id = ANY($1)");

    insert_results(m.exec_prepared("relation_parents_of_nodes", sel_nodes),
                   sel_relations);
  }
}

void readonly_pgsql_selection::select_relations_from_relations(bool drop_relations) {
  if (!sel_relations.empty()) {

    std::set<osm_nwr_id_t> sel;
    if (drop_relations)
      sel_relations.swap(sel);
    else
      sel = sel_relations;

    m.prepare("relation_parents_of_relations",
       "SELECT DISTINCT rm.relation_id AS id "
         "FROM current_relation_members rm "
         "WHERE rm.member_type = 'Relation' "
           "AND rm.member_id = ANY($1)");

    insert_results(
        m.exec_prepared("relation_parents_of_relations", sel),
        sel_relations);
  }
}

void readonly_pgsql_selection::select_relations_members_of_relations() {
  if (!sel_relations.empty()) {
    m.prepare("relation_members_of_relations",
       "SELECT DISTINCT rm.member_id AS id "
	 "FROM current_relation_members rm "
	 "WHERE rm.member_type = 'Relation' "
	   "AND rm.relation_id = ANY($1)");

    insert_results(
        m.exec_prepared("relation_members_of_relations", sel_relations),
        sel_relations);
  }
}

int readonly_pgsql_selection::select_historical_nodes(
  const std::vector<osm_edition_t> &eds) {

  if (eds.empty())
    return 0;

  m.prepare("select_historical_nodes",
     "WITH wanted(id, version) AS ("
       "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
     ")"
     "SELECT n.node_id AS id, n.version "
       "FROM nodes n "
       "INNER JOIN wanted w ON n.node_id = w.id AND n.version = w.version "
       "WHERE (n.redaction_id IS NULL OR $3 = TRUE)");

  std::vector<osm_nwr_id_t> ids(eds.size());
  std::vector<osm_version_t> vers(eds.size());

  for (const auto &ed : eds) {
    ids.emplace_back(ed.first);
    vers.emplace_back(ed.second);
  }

  return insert_results(
    m.exec_prepared("select_historical_nodes", ids, vers, m_redactions_visible),
    sel_historic_nodes);
}

int readonly_pgsql_selection::select_historical_ways(
  const std::vector<osm_edition_t> &eds) {

  if (eds.empty())
    return 0;

  m.prepare("select_historical_ways",
     "WITH wanted(id, version) AS ("
       "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
     ")"
     "SELECT w.way_id AS id, w.version "
       "FROM ways w "
       "INNER JOIN wanted x ON w.way_id = x.id AND w.version = x.version "
       "WHERE (w.redaction_id IS NULL OR $3 = TRUE)");

  std::vector<osm_nwr_id_t> ids(eds.size());
  std::vector<osm_version_t> vers(eds.size());

  for (const auto &ed : eds) {
    ids.emplace_back(ed.first);
    vers.emplace_back(ed.second);
  }

  return insert_results(
    m.exec_prepared("select_historical_ways", ids, vers, m_redactions_visible),
    sel_historic_ways);
}

int readonly_pgsql_selection::select_historical_relations(
  const std::vector<osm_edition_t> &eds) {

  if (eds.empty())
    return 0;

  m.prepare("select_historical_relations",
     "WITH wanted(id, version) AS ("
       "SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))"
     ")"
     "SELECT r.relation_id AS id, r.version "
       "FROM relations r "
       "INNER JOIN wanted x ON r.relation_id = x.id AND r.version = x.version "
       "WHERE (r.redaction_id IS NULL OR $3 = TRUE)");

  std::vector<osm_nwr_id_t> ids(eds.size());
  std::vector<osm_version_t> vers(eds.size());

  for (const auto &ed : eds) {
    ids.emplace_back(ed.first);
    vers.emplace_back(ed.second);
  }

  return insert_results(
    m.exec_prepared("select_historical_relations", ids, vers, m_redactions_visible),
    sel_historic_relations);
}

int readonly_pgsql_selection::select_nodes_with_history(
  const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return 0;

  m.prepare("select_nodes_history",
     "SELECT node_id AS id, version "
       "FROM nodes "
       "WHERE node_id = ANY($1) AND "
             "(redaction_id IS NULL OR $2 = TRUE)");

  return insert_results(
    m.exec_prepared("select_nodes_history", ids, m_redactions_visible),
    sel_historic_nodes);
}

int readonly_pgsql_selection::select_ways_with_history(
  const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return 0;

  m.prepare("select_ways_history",
     "SELECT way_id AS id, version "
       "FROM ways "
       "WHERE way_id = ANY($1) AND "
             "(redaction_id IS NULL OR $2 = TRUE)");

  return insert_results(
    m.exec_prepared("select_ways_history", ids, m_redactions_visible),
    sel_historic_ways);
}

int readonly_pgsql_selection::select_relations_with_history(
  const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return 0;

  m.prepare("select_relations_history",
     "SELECT relation_id AS id, version "
       "FROM relations "
       "WHERE relation_id = ANY($1) AND "
             "(redaction_id IS NULL OR $2 = TRUE)");

  return insert_results(m.exec_prepared("select_relations_history", ids, m_redactions_visible),
    sel_historic_relations);
}

void readonly_pgsql_selection::set_redactions_visible(bool visible) {
  m_redactions_visible = visible;
}

int readonly_pgsql_selection::select_historical_by_changesets(
  const std::vector<osm_changeset_id_t> &ids) {

  if (ids.empty())
    return 0;

  m.prepare("select_nodes_by_changesets",
       "SELECT n.node_id AS id, n.version "
       "FROM nodes n "
       "WHERE n.changeset_id = ANY($1) "
         "AND (n.redaction_id IS NULL OR $2 = TRUE)");

  m.prepare("select_ways_by_changesets",
      "SELECT w.way_id AS id, w.version "
      "FROM ways w "
      "WHERE w.changeset_id = ANY($1) "
        "AND (w.redaction_id IS NULL OR $2 = TRUE)");

  m.prepare("select_relations_by_changesets",
      "SELECT r.relation_id AS id, r.version "
      "FROM relations r "
      "WHERE r.changeset_id = ANY($1) "
        "AND (r.redaction_id IS NULL OR $2 = TRUE)");


  int selected = insert_results(m.exec_prepared("select_nodes_by_changesets", ids, m_redactions_visible),
    sel_historic_nodes);
  selected += insert_results(m.exec_prepared("select_ways_by_changesets", ids, m_redactions_visible),
    sel_historic_ways);
  selected += insert_results(m.exec_prepared("select_relations_by_changesets", ids, m_redactions_visible),
    sel_historic_relations);

  return selected;
}

void readonly_pgsql_selection::drop_nodes() {
  sel_nodes.clear();
}

void readonly_pgsql_selection::drop_ways() {
  sel_ways.clear();
}

void readonly_pgsql_selection::drop_relations() {
  sel_relations.clear();
}


int readonly_pgsql_selection::select_changesets(const std::vector<osm_changeset_id_t> &ids) {

  if (ids.empty())
    return 0;

  m.prepare("select_changesets",
     "SELECT id "
       "FROM changesets "
       "WHERE id = ANY($1)");

  return insert_results(m.exec_prepared("select_changesets", ids), sel_changesets);
}

void readonly_pgsql_selection::select_changeset_discussions() {
  include_changeset_discussions = true;
}

bool readonly_pgsql_selection::supports_user_details() const {
  return true;
}

bool readonly_pgsql_selection::is_user_blocked(const osm_user_id_t id) {

  m.prepare("check_user_blocked",
    R"(SELECT id FROM "user_blocks" 
          WHERE "user_blocks"."user_id" = $1 
            AND (needs_view or ends_at > (now() at time zone 'utc')) LIMIT 1 )");

  auto res = m.exec_prepared("check_user_blocked", id);
  return !res.empty();
}

bool readonly_pgsql_selection::get_user_id_pass(const std::string& user_name, osm_user_id_t & id,
						 std::string & pass_crypt, std::string & pass_salt) {

  std::string email = boost::algorithm::trim_copy(user_name);

  m.prepare("get_user_id_pass",
    R"(SELECT id, pass_crypt, COALESCE(pass_salt, '') as pass_salt FROM users
           WHERE (email = $1 OR display_name = $2)
             AND (status = 'active' or status = 'confirmed') LIMIT 1
      )");

  m.prepare("get_user_id_pass_case_insensitive",
    R"(SELECT id, pass_crypt, COALESCE(pass_salt, '') as pass_salt FROM users
           WHERE (LOWER(email) = LOWER($1) OR LOWER(display_name) = LOWER($2))
             AND (status = 'active' or status = 'confirmed')
      )");


  auto res = m.exec_prepared("get_user_id_pass", email, user_name);

  if (res.empty()) {
    // try case insensitive query
    res = m.exec_prepared("get_user_id_pass_case_insensitive", email, user_name);
    // failure, in case no entries or multiple entries were found
    if (res.size() != 1)
      return false;
  }

  auto row = res[0];
  id = row["id"].as<osm_user_id_t>();
  pass_crypt = row["pass_crypt"].as<std::string>();
  pass_salt = row["pass_salt"].as<std::string>();

  return true;
}

readonly_pgsql_selection::factory::factory(const po::variables_map &opts)
    : m_connection(connect_db_str(opts)),
      m_cache_connection(connect_db_str(opts)),
      m_errorhandler(m_connection),
      m_cache_errorhandler(m_cache_connection),
      m_cache_tx(m_cache_connection, "changeset_cache"),
      m_cache(std::bind(fetch_changesets, std::ref(m_cache_tx), std::placeholders::_1),
              opts["cachesize"].as<size_t>()) {

  check_postgres_version(m_connection);

  // set the connections to use the appropriate charset.
  m_connection.set_client_encoding(opts["charset"].as<std::string>());
  m_cache_connection.set_client_encoding(opts["charset"].as<std::string>());

  // set the connection to use readonly transaction.
  m_connection.set_variable("default_transaction_read_only", "true");
}

readonly_pgsql_selection::factory::~factory() = default;


std::unique_ptr<data_selection>
readonly_pgsql_selection::factory::make_selection(Transaction_Owner_Base& to) {
  return std::make_unique<readonly_pgsql_selection>(to,
						    std::ref(m_cache));
}

std::unique_ptr<Transaction_Owner_Base>
readonly_pgsql_selection::factory::get_default_transaction()
{
  return std::make_unique<Transaction_Owner_ReadOnly>(std::ref(m_connection));
}


