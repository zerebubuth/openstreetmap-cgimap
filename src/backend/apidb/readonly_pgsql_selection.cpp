/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
#include "cgimap/backend/apidb/common_pgsql_selection.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/options.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"

#include <functional>
#include <set>
#include <sstream>
#include <vector>

namespace po = boost::program_options;


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

template <typename T> T id_of(const pqxx_tuple &, pqxx::row_size_type col);

template <>
osm_nwr_id_t id_of<osm_nwr_id_t>(const pqxx_tuple &row, pqxx::row_size_type col) {
  return row[col].as<osm_nwr_id_t>();
}

template <>
osm_changeset_id_t id_of<osm_changeset_id_t>(const pqxx_tuple &row, pqxx::row_size_type col) {
  return row[col].as<osm_changeset_id_t>();
}

template <>
osm_edition_t id_of<osm_edition_t>(const pqxx_tuple &row, pqxx::row_size_type col) {
  auto id = row[col].as<osm_nwr_id_t>();
  auto ver = row["version"].as<osm_version_t>();
  return {id, ver};
}

template <typename T>
inline int insert_results(const pqxx::result &res, std::set<T> &elems) {

  auto const id_col = res.column_number("id");

  const auto old_size = elems.size();

  auto it = elems.begin();
  for (const auto & row : res) {
    const T id = id_of<T>(row, id_col);
    it = elems.emplace_hint(it, id);
  }

  return elems.size() - old_size; // number of inserted elements
}

} // anonymous namespace

readonly_pgsql_selection::readonly_pgsql_selection(
    Transaction_Owner_Base& to)
    : m(to) {}

void readonly_pgsql_selection::write_nodes(output_formatter &formatter) {

  if (!sel_nodes.empty() && !sel_historic_nodes.empty()) {
    // both current nodes and historic nodes were selected,
    // lookup object versions, and handle request via historic nodes

    logger::message("Fetching current_node versions");

    m.prepare("lookup_node_versions",
        R"(SELECT n.id, n.version
           FROM current_nodes n
           WHERE n.id = ANY($1)
        )"_M);

    auto res = m.exec_prepared("lookup_node_versions", sel_nodes);

    for (const auto & row : res) {
      sel_historic_nodes.insert({row[0].as<osm_nwr_id_t>(), row[1].as<osm_version_t>()});
    }
    sel_nodes.clear();
  }

  logger::message("Fetching nodes");

  // get all nodes - they already contain their own tags, so
  // we don't need to do anything else.
  if (!sel_nodes.empty()) {

    m.prepare("extract_nodes",
      R"(SELECT n.id, n.latitude, n.longitude, n.visible,
          to_char(n.timestamp,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS timestamp,
          n.changeset_id, n.version,
          array_agg(t.k ORDER BY k) as tag_k,
          array_agg(t.v ORDER BY k) as tag_v
        FROM current_nodes n
          LEFT JOIN current_node_tags t ON n.id=t.node_id
        WHERE n.id = ANY($1)
        GROUP BY n.id ORDER BY n.id)"_M);

    auto result = m.exec_prepared("extract_nodes", sel_nodes);
    fetch_changesets(extract_changeset_ids(result), cc);
    extract_nodes(result, formatter, cc);
  }
  else if (!sel_historic_nodes.empty()) {

    m.prepare("extract_historic_nodes",
      R"(WITH wanted(id, version) AS (
        SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))
      )
      SELECT n.node_id AS id, n.latitude, n.longitude, n.visible,
          to_char(n.timestamp,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS timestamp,
          n.changeset_id, n.version,
          array_agg(t.k ORDER BY k) as tag_k,
          array_agg(t.v ORDER BY k) as tag_v
        FROM nodes n
          INNER JOIN wanted x ON n.node_id = x.id AND n.version = x.version
          LEFT JOIN node_tags t ON n.node_id = t.node_id AND n.version = t.version
        GROUP BY n.node_id, n.version ORDER BY n.node_id, n.version)"_M);

    std::vector<osm_nwr_id_t> ids;
    std::vector<osm_nwr_id_t> versions;

    for (const auto &[id, version] : sel_historic_nodes) {
      ids.emplace_back(id);
      versions.emplace_back(version);
    }

    auto result = m.exec_prepared("extract_historic_nodes", ids, versions);
    fetch_changesets(extract_changeset_ids(result), cc);
    extract_nodes(result, formatter, cc);
  }
}

void readonly_pgsql_selection::write_ways(output_formatter &formatter) {

  if (!sel_ways.empty() && !sel_historic_ways.empty()) {
    // both current ways and historic ways were selected,
    // lookup object versions, and handle request via historic ways

    logger::message("Fetching current_way versions");

    m.prepare("lookup_way_versions",
        R"(SELECT w.id, w.version
           FROM current_ways w
           WHERE w.id = ANY($1)
        )"_M);

    auto res = m.exec_prepared("lookup_way_versions", sel_ways);

    for (const auto & row : res) {
      sel_historic_ways.insert({row[0].as<osm_nwr_id_t>(), row[1].as<osm_version_t>()});
    }
    sel_ways.clear();
  }

  // grab the ways, way nodes and tags
  // way nodes and tags are on a separate connections so that the
  // entire result set can be streamed from a single query.

  logger::message("Fetching ways");

  if (!sel_ways.empty()) {

    m.prepare("extract_ways",
      R"(SELECT w.id, w.visible,
          to_char(w.timestamp,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS timestamp,
          w.changeset_id, w.version, t.keys as tag_k, t.values as tag_v,
          wn.node_ids as node_ids
        FROM current_ways w
          LEFT JOIN LATERAL
            (SELECT array_agg(k ORDER BY k) as keys,
                    array_agg(v ORDER BY k) as values
              FROM current_way_tags
              WHERE w.id=way_id) t ON true
          LEFT JOIN LATERAL
            (SELECT array_agg(node_id) as node_ids
              FROM
                (SELECT node_id FROM current_way_nodes WHERE w.id=way_id
                ORDER BY sequence_id) x) wn ON true
        WHERE w.id = ANY($1)
        ORDER BY w.id)"_M);

    auto result = m.exec_prepared("extract_ways", sel_ways);
    fetch_changesets(extract_changeset_ids(result), cc);
    extract_ways(result, formatter, cc);
  }
  else if (!sel_historic_ways.empty()) {

    m.prepare("extract_historic_ways",
      R"(WITH wanted(id, version) AS (
        SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))
      )
      SELECT w.way_id AS id, w.visible,
          to_char(w.timestamp,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS timestamp,
          w.changeset_id, w.version, t.keys as tag_k, t.values as tag_v,
          wn.node_ids as node_ids
        FROM ways w
          INNER JOIN wanted x ON w.way_id = x.id AND w.version = x.version
          LEFT JOIN LATERAL
            (SELECT array_agg(k ORDER BY k) as keys,
                    array_agg(v ORDER BY k) as values
              FROM way_tags
              WHERE w.way_id=way_id AND w.version=version) t ON true
          LEFT JOIN LATERAL
            (SELECT array_agg(node_id) as node_ids
              FROM
                (SELECT node_id FROM way_nodes
                WHERE w.way_id=way_id AND w.version=version
                ORDER BY sequence_id) x) wn ON true
        ORDER BY w.way_id, w.version)"_M);

    std::vector<osm_nwr_id_t> ids;
    std::vector<osm_nwr_id_t> versions;
    ids.reserve(sel_historic_ways.size());
    versions.reserve(sel_historic_ways.size());

    for (const auto &[id, version] : sel_historic_ways) {
      ids.emplace_back(id);
      versions.emplace_back(version);
    }

    auto result = m.exec_prepared("extract_historic_ways", ids, versions);
    fetch_changesets(extract_changeset_ids(result), cc);
    extract_ways(result, formatter, cc);
  }
}

void readonly_pgsql_selection::write_relations(output_formatter &formatter) {

  if (!sel_relations.empty() && !sel_historic_relations.empty()) {
    // both current relation and historic relation were selected,
    // lookup object versions, and handle request via historic relations

    logger::message("Fetching current_relation versions");

    m.prepare("lookup_relation_versions",
        R"(SELECT r.id, r.version
           FROM current_relations r
           WHERE r.id = ANY($1)
        )"_M);

    auto res = m.exec_prepared("lookup_relation_versions", sel_relations);

    for (const auto & row : res) {
      sel_historic_relations.insert({row[0].as<osm_nwr_id_t>(), row[1].as<osm_version_t>()});
    }
    sel_relations.clear();
  }

  logger::message("Fetching relations");

  if (!sel_relations.empty()) {

    m.prepare("extract_relations",
      R"(SELECT r.id, r.visible,
          to_char(r.timestamp,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS timestamp,
          r.changeset_id, r.version, t.keys as tag_k, t.values as tag_v,
          rm.types as member_types, rm.ids as member_ids, rm.roles as member_roles
        FROM current_relations r
          LEFT JOIN LATERAL
            (SELECT array_agg(k ORDER BY k) as keys,
                    array_agg(v ORDER BY k) as values
              FROM current_relation_tags
              WHERE r.id=relation_id) t ON true
          LEFT JOIN LATERAL
            (SELECT array_agg(member_type) as types,
              array_agg(member_role) as roles, array_agg(member_id) as ids
              FROM
                (SELECT * FROM current_relation_members WHERE r.id=relation_id
                ORDER BY sequence_id) x) rm ON true
        WHERE r.id = ANY($1)
        ORDER BY r.id)"_M);

    auto result = m.exec_prepared("extract_relations", sel_relations);

    fetch_changesets(extract_changeset_ids(result), cc);
    extract_relations(result, formatter, cc);
  }
  else if (!sel_historic_relations.empty()) {

    m.prepare("extract_historic_relations",
      R"(WITH wanted(id, version) AS (
        SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))
      )
      SELECT r.relation_id AS id, r.visible,
          to_char(r.timestamp,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS timestamp,
          r.changeset_id, r.version, t.keys as tag_k, t.values as tag_v,
          rm.types as member_types, rm.ids as member_ids, rm.roles as member_roles
        FROM relations r
          INNER JOIN wanted x ON r.relation_id = x.id AND r.version = x.version
          LEFT JOIN LATERAL
            (SELECT array_agg(k ORDER BY k) as keys,
                    array_agg(v ORDER BY k) as values
              FROM relation_tags
              WHERE r.relation_id=relation_id AND r.version=version) t ON true
          LEFT JOIN LATERAL
            (SELECT array_agg(member_type) as types,
              array_agg(member_role) as roles, array_agg(member_id) as ids
              FROM
                (SELECT * FROM relation_members WHERE r.relation_id=relation_id AND r.version=version
                ORDER BY sequence_id) x) rm ON true
        ORDER BY r.relation_id, r.version)"_M);

    std::vector<osm_nwr_id_t> ids;
    std::vector<osm_nwr_id_t> versions;
    ids.reserve(sel_historic_relations.size());
    versions.reserve(sel_historic_relations.size());

    for (const auto &[id, version] : sel_historic_relations) {
      ids.emplace_back(id);
      versions.emplace_back(version);
    }

    auto result = m.exec_prepared("extract_historic_relations", ids, versions);
    fetch_changesets(extract_changeset_ids(result), cc);
    extract_relations(result, formatter, cc);
  }
}

void readonly_pgsql_selection::write_changesets(output_formatter &formatter,
                                                const std::chrono::system_clock::time_point &now) {

  if (sel_changesets.empty())
    return;

  m.prepare("extract_changesets",
      R"(SELECT c.id,
        to_char(c.created_at,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS created_at,
        to_char(c.closed_at, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS closed_at,
        c.min_lat, c.max_lat, c.min_lon, c.max_lon,
        c.num_changes,
        t.keys as tag_k, t.values as tag_v,
        cc.id as comment_id,
        cc.author_id as comment_author_id,
        cc.display_name as comment_display_name,
        cc.body as comment_body,
        cc.created_at as comment_created_at
      FROM changesets c
       LEFT JOIN LATERAL
           (SELECT array_agg(k ORDER BY k) AS keys,
                   array_agg(v ORDER BY k) AS values
           FROM changeset_tags
           WHERE c.id=changeset_id ) t ON true
       LEFT JOIN LATERAL
         (SELECT array_agg(id) as id,
         array_agg(author_id) as author_id,
         array_agg(display_name) as display_name,
         array_agg(body) as body,
         array_agg(created_at) as created_at FROM
           (SELECT cc.id, cc.author_id, u.display_name, cc.body,
           to_char(cc.created_at,'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS created_at
           FROM changeset_comments cc JOIN users u ON cc.author_id = u.id
           where cc.changeset_id=c.id AND cc.visible ORDER BY cc.created_at) x
         )cc ON true
      WHERE c.id = ANY($1))"_M);

  pqxx::result changesets = m.exec_prepared("extract_changesets", sel_changesets);

  fetch_changesets(sel_changesets, cc);

  extract_changesets(changesets, formatter, cc, now, include_changeset_discussions);
}

data_selection::visibility_t
readonly_pgsql_selection::check_node_visibility(osm_nwr_id_t id) {
  m.prepare("visible_node",
     R"(SELECT visible FROM current_nodes WHERE id = $1)");
  return check_table_visibility(m, id, "visible_node");
}

data_selection::visibility_t
readonly_pgsql_selection::check_way_visibility(osm_nwr_id_t id) {
  m.prepare("visible_way",
     R"(SELECT visible FROM current_ways WHERE id = $1)");
  return check_table_visibility(m, id, "visible_way");
}

data_selection::visibility_t
readonly_pgsql_selection::check_relation_visibility(osm_nwr_id_t id) {

  m.prepare("visible_relation", R"(SELECT visible FROM current_relations WHERE id = $1)");

  return check_table_visibility(m, id, "visible_relation");
}

int readonly_pgsql_selection::select_nodes(const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return 0;

  m.prepare("select_nodes", R"(SELECT id FROM current_nodes WHERE id = ANY($1))");

  return insert_results(m.exec_prepared("select_nodes", ids), sel_nodes);
}

int readonly_pgsql_selection::select_ways(const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return 0;

  m.prepare("select_ways", R"(SELECT id FROM current_ways WHERE id = ANY($1))");

  return insert_results(m.exec_prepared("select_ways", ids), sel_ways);
}

int readonly_pgsql_selection::select_relations(const std::vector<osm_nwr_id_t> &ids) {
  if (ids.empty())
    return 0;

  m.prepare("select_relations", R"(SELECT id FROM current_relations WHERE id = ANY($1))");

  return insert_results(m.exec_prepared("select_relations", ids),
                        sel_relations);
}

int readonly_pgsql_selection::select_nodes_from_bbox(const bbox &bounds,
                                                     int max_nodes) {
  const std::vector<tile_id_t> tiles = tiles_for_area(
      bounds.minlat, bounds.minlon, bounds.maxlat, bounds.maxlon);

  // select nodes with bbox
 m.prepare("visible_node_in_bbox",
    R"(SELECT id
      FROM current_nodes
      WHERE tile = ANY($1)
        AND latitude BETWEEN $2 AND $3
        AND longitude BETWEEN $4 AND $5
        AND visible = true
      LIMIT $6)"_M);

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
       R"(SELECT DISTINCT rm.member_id AS id
	 FROM current_relation_members rm
	 WHERE rm.member_type = 'Node'
	   AND rm.relation_id = ANY($1) ORDER by id)"_M);

    insert_results(m.exec_prepared("nodes_from_relations", sel_relations),
                   sel_nodes);
  }
}

void readonly_pgsql_selection::select_ways_from_nodes() {
  logger::message("Filling sel_ways (from nodes)");

  if (!sel_nodes.empty()) {
    m.prepare("ways_from_nodes",
       R"(SELECT DISTINCT wn.way_id AS id
	 FROM current_way_nodes wn
	 WHERE wn.node_id = ANY($1) ORDER by id)"_M);

    insert_results(m.exec_prepared("ways_from_nodes", sel_nodes), sel_ways);
  }
}

void readonly_pgsql_selection::select_ways_from_relations() {
  logger::message("Filling sel_ways (from relations)");

  if (!sel_relations.empty()) {
    m.prepare("ways_from_relations",
       R"(SELECT DISTINCT rm.member_id AS id
	 FROM current_relation_members rm
	 WHERE rm.member_type = 'Way'
	   AND rm.relation_id = ANY($1) ORDER by id)"_M);

    insert_results(m.exec_prepared("ways_from_relations", sel_relations),
                   sel_ways);
  }
}

void readonly_pgsql_selection::select_relations_from_ways() {
  logger::message("Filling sel_relations (from ways)");

  if (!sel_ways.empty()) {
    m.prepare("relation_parents_of_ways",
       R"(SELECT DISTINCT rm.relation_id AS id
	 FROM current_relation_members rm
	 WHERE rm.member_type = 'Way'
	   AND rm.member_id = ANY($1) ORDER by id)"_M);

    insert_results(m.exec_prepared("relation_parents_of_ways", sel_ways),
                   sel_relations);
  }
}

void readonly_pgsql_selection::select_nodes_from_way_nodes() {
  if (!sel_ways.empty()) {
    m.prepare("nodes_from_ways",
       R"(SELECT DISTINCT wn.node_id AS id
	 FROM current_way_nodes wn
	 WHERE wn.way_id = ANY($1) ORDER by id)"_M);

    insert_results(m.exec_prepared("nodes_from_ways", sel_ways), sel_nodes);
  }
}

void readonly_pgsql_selection::select_relations_from_nodes() {
  if (!sel_nodes.empty()) {
    m.prepare("relation_parents_of_nodes",
       R"(SELECT DISTINCT rm.relation_id AS id
	 FROM current_relation_members rm
	 WHERE rm.member_type = 'Node'
	   AND rm.member_id = ANY($1) ORDER by id)"_M);

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
       R"(SELECT DISTINCT rm.relation_id AS id
         FROM current_relation_members rm
         WHERE rm.member_type = 'Relation'
           AND rm.member_id = ANY($1) ORDER by id)"_M);

    insert_results(
        m.exec_prepared("relation_parents_of_relations", sel),
        sel_relations);
  }
}

void readonly_pgsql_selection::select_relations_members_of_relations() {
  if (!sel_relations.empty()) {
    m.prepare("relation_members_of_relations",
       R"(SELECT DISTINCT rm.member_id AS id
	 FROM current_relation_members rm
	 WHERE rm.member_type = 'Relation'
	   AND rm.relation_id = ANY($1) ORDER by id)"_M);

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
     R"(WITH wanted(id, version) AS (
       SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))
     )
     SELECT n.node_id AS id, n.version
       FROM nodes n
       INNER JOIN wanted w ON n.node_id = w.id AND n.version = w.version
       WHERE (n.redaction_id IS NULL OR $3 = TRUE))"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_version_t> vers;
  ids.reserve(eds.size());
  vers.reserve(eds.size());

  for (const auto &[id, version] : eds) {
    ids.emplace_back(id);
    vers.emplace_back(version);
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
     R"(WITH wanted(id, version) AS (
       SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))
     )
     SELECT w.way_id AS id, w.version
       FROM ways w
       INNER JOIN wanted x ON w.way_id = x.id AND w.version = x.version
       WHERE (w.redaction_id IS NULL OR $3 = TRUE))"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_version_t> vers;
  ids.reserve(eds.size());
  vers.reserve(eds.size());

  for (const auto &[id, version] : eds) {
    ids.emplace_back(id);
    vers.emplace_back(version);
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
     R"(WITH wanted(id, version) AS (
       SELECT * FROM unnest(CAST($1 AS bigint[]), CAST($2 AS bigint[]))
     )
     SELECT r.relation_id AS id, r.version
       FROM relations r
       INNER JOIN wanted x ON r.relation_id = x.id AND r.version = x.version
       WHERE (r.redaction_id IS NULL OR $3 = TRUE))"_M);

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_version_t> vers;
  ids.reserve(eds.size());
  vers.reserve(eds.size());

  for (const auto &[id, version] : eds) {
    ids.emplace_back(id);
    vers.emplace_back(version);
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
     R"(SELECT node_id AS id, version
       FROM nodes
       WHERE node_id = ANY($1) AND
             (redaction_id IS NULL OR $2 = TRUE))"_M);

  return insert_results(
    m.exec_prepared("select_nodes_history", ids, m_redactions_visible),
    sel_historic_nodes);
}

int readonly_pgsql_selection::select_ways_with_history(
  const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return 0;

  m.prepare("select_ways_history",
     R"(SELECT way_id AS id, version
       FROM ways
       WHERE way_id = ANY($1) AND
             (redaction_id IS NULL OR $2 = TRUE))"_M);

  return insert_results(
    m.exec_prepared("select_ways_history", ids, m_redactions_visible),
    sel_historic_ways);
}

int readonly_pgsql_selection::select_relations_with_history(
  const std::vector<osm_nwr_id_t> &ids) {

  if (ids.empty())
    return 0;

  m.prepare("select_relations_history",
     R"(SELECT relation_id AS id, version
       FROM relations
       WHERE relation_id = ANY($1) AND
             (redaction_id IS NULL OR $2 = TRUE))"_M);

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
       R"(SELECT n.node_id AS id, n.version
       FROM nodes n
       WHERE n.changeset_id = ANY($1)
         AND (n.redaction_id IS NULL OR $2 = TRUE))"_M);

  m.prepare("select_ways_by_changesets",
      R"(SELECT w.way_id AS id, w.version
      FROM ways w
      WHERE w.changeset_id = ANY($1)
        AND (w.redaction_id IS NULL OR $2 = TRUE))"_M);

  m.prepare("select_relations_by_changesets",
      R"(SELECT r.relation_id AS id, r.version
      FROM relations r
      WHERE r.changeset_id = ANY($1)
        AND (r.redaction_id IS NULL OR $2 = TRUE))"_M);


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
     R"(SELECT id
       FROM changesets
       WHERE id = ANY($1))"_M);

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
            AND (needs_view or ends_at > (now() at time zone 'utc')) LIMIT 1 )"_M);

  auto res = m.exec_prepared("check_user_blocked", id);
  return !res.empty();
}

std::set< osm_user_role_t > readonly_pgsql_selection::get_roles_for_user(osm_user_id_t id)
{
  std::set<osm_user_role_t> roles;

  // return all the roles to which the user belongs.
  m.prepare("roles_for_user",
    R"(SELECT role FROM user_roles WHERE user_id = $1)");

  auto res = m.exec_prepared("roles_for_user", id);

  for (const auto &tuple : res) {
    auto role = tuple[0].as<std::string>();
    if (role == "moderator") {
      roles.insert(osm_user_role_t::moderator);
    } else if (role == "administrator") {
      roles.insert(osm_user_role_t::administrator);
    } else if (role == "importer") {
      roles.insert(osm_user_role_t::importer);
    }
  }

  return roles;
}

std::optional< osm_user_id_t > readonly_pgsql_selection::get_user_id_for_oauth2_token(
    const std::string &token_id, bool &expired, bool &revoked,
    bool &allow_api_write)
{
  // return details for OAuth 2.0 access token
  m.prepare("oauth2_access_token",
    R"(SELECT resource_owner_id as user_id,
         CASE WHEN expires_in IS NULL THEN false
              ELSE (created_at + expires_in * interval '1' second) < now() at time zone 'utc'
         END as expired,
         COALESCE(revoked_at < now() at time zone 'utc', false) as revoked,
         'write_api' = any(string_to_array(coalesce(scopes,''), ' ')) as allow_api_write
       FROM oauth_access_tokens
       WHERE token = $1)"_M);

  auto res = m.exec_prepared("oauth2_access_token", token_id);

  if (!res.empty()) {
    auto uid = res[0]["user_id"].as<osm_user_id_t>();
    expired = res[0]["expired"].as<bool>();
    revoked = res[0]["revoked"].as<bool>();
    allow_api_write = res[0]["allow_api_write"].as<bool>();
    return uid;

  } else {
    expired = true;
    revoked = true;
    allow_api_write = false;
    return {};
  }
}

bool readonly_pgsql_selection::is_user_active(const osm_user_id_t id)
{
  m.prepare("is_user_active",
         R"(SELECT id FROM users
            WHERE id = $1
            AND (status = 'active' or status = 'confirmed'))"_M);

  auto res = m.exec_prepared("is_user_active", id);
  return (!res.empty());
}

std::set< osm_changeset_id_t > readonly_pgsql_selection::extract_changeset_ids(const pqxx::result& result) const {

  std::set< osm_changeset_id_t > changeset_ids;
  auto const changeset_id_col = result.column_number("changeset_id");

  for (const auto & row : result) {
    changeset_ids.insert(row[changeset_id_col].as<osm_changeset_id_t>());
  }
  return changeset_ids;
}

void readonly_pgsql_selection::fetch_changesets(const std::set< osm_changeset_id_t >& all_ids, std::map<osm_changeset_id_t, changeset>& cc ) {

  std::set< osm_changeset_id_t> ids;

  // check if changeset is already contained in map
  for (auto id: all_ids) {
    if (!cc.contains(id)) {
      ids.insert(id);
    }
  }

  if (ids.empty())
    return;

  m.prepare("extract_changeset_userdetails",
      R"(SELECT c.id, u.data_public, u.display_name, u.id from users u
                   join changesets c on u.id=c.user_id where c.id = ANY($1))"_M);

  pqxx::result res = m.exec_prepared("extract_changeset_userdetails", ids);

  for (const auto & r : res) {

    auto cs = r[0].as<int64_t>();

    // Multiple results for one changeset?
    if (cc.contains(cs)) {
      logger::message(
          fmt::format("ERROR: Request for user data associated with changeset {:d} failed: returned multiple rows.", cs));
      throw http::server_error(
          fmt::format("Possible database inconsistency with changeset {:d}.", cs));
    }

    auto user_id = r[3].as<int64_t>();
    // apidb instances external to OSM don't have access to anonymous
    // user information and so use an ID which isn't in use for any
    // other user to indicate this - generally 0 or negative.
    if (user_id <= 0) {
      cc[cs] = changeset{false, "", 0};
    } else {
      cc[cs] = changeset{r[1].as<bool>(), r[2].as<std::string>(), osm_user_id_t(user_id)};
    }
  }

  // although the above query should always return one row, it might
  // happen that we get a weird changeset ID from somewhere, or the
  // FK constraints might have failed. in this situation all we can
  // really do is whine loudly and bail.

  // Missing changeset in query result?
  for (const auto & id : ids) {
    if (!cc.contains(id)) {
      logger::message(
          fmt::format("ERROR: Request for user data associated with changeset {:d} failed: returned 0 rows.", id));
      throw http::server_error(
          fmt::format("Possible database inconsistency with changeset {:d}.", id));
    }
  }
}

readonly_pgsql_selection::factory::factory(const po::variables_map &opts)
    : m_connection(connect_db_str(opts)),
      m_errorhandler(m_connection) {

  check_postgres_version(m_connection);

  // set the connections to use the appropriate charset.
  m_connection.set_client_encoding("utf8");

#if PQXX_VERSION_MAJOR < 7
  // set the connection to use readonly transaction.
  m_connection.set_variable("default_transaction_read_only", "true");
#else
  m_connection.set_session_var("default_transaction_read_only", "true");
#endif
}


std::unique_ptr<data_selection>
readonly_pgsql_selection::factory::make_selection(Transaction_Owner_Base& to) const {
  return std::make_unique<readonly_pgsql_selection>(to);
}

std::unique_ptr<Transaction_Owner_Base>
readonly_pgsql_selection::factory::get_default_transaction()
{
  return std::make_unique<Transaction_Owner_ReadOnly>(std::ref(m_connection), m_prep_stmt);
}
