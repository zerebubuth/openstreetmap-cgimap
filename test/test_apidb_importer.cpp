/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "test_apidb_importer.hpp"
#include "cgimap/api06/id_version.hpp"
#include "cgimap/backend/apidb/pqxx_string_traits.hpp"
#include "cgimap/backend/apidb/quad_tile.hpp"
#include "cgimap/backend/apidb/utils.hpp"

#include <fmt/core.h>
#include <iostream>
#include <set>
#include <vector>

void copy_nodes_to_current_nodes(Transaction_Manager &m) {

  m.exec(R"(

    WITH max_versions AS (
        SELECT node_id, MAX(version) AS max_version
        FROM nodes
        GROUP BY node_id
    ),
    S1 AS (
      -- Insert into current_nodes
      INSERT INTO current_nodes (id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
      SELECT n.node_id, n.latitude, n.longitude, n.changeset_id, n.visible, n."timestamp", n.tile, n.version
      FROM nodes n
      JOIN max_versions mv
      ON n.node_id = mv.node_id AND n.version = mv.max_version
    ),
    S2 AS (
      -- Insert into current_node_tags
      INSERT INTO current_node_tags (node_id, k, v)
      SELECT nt.node_id, nt.k, nt.v
      FROM node_tags nt
      JOIN max_versions mv
      ON nt.node_id = mv.node_id AND nt.version = mv.max_version )
    SELECT TRUE;
  )");
}

void copy_ways_to_current_ways(Transaction_Manager &m) {

  m.exec(R"(

  WITH max_versions AS (
      SELECT way_id, MAX(version) AS max_version
      FROM ways
      GROUP BY way_id
  ),
  S1 AS (
    -- Insert into current_ways
    INSERT INTO current_ways (id, changeset_id, "timestamp", visible, version)
    SELECT w.way_id, w.changeset_id, w."timestamp", w.visible, w.version
    FROM ways w
    JOIN max_versions mv ON w.way_id = mv.way_id AND w.version = mv.max_version
  ),
  S2 AS (
    -- Insert into current_way_nodes
    INSERT INTO current_way_nodes (way_id, node_id, sequence_id)
    SELECT wn.way_id, wn.node_id, wn.sequence_id
    FROM way_nodes wn
    JOIN max_versions mv
    ON wn.way_id = mv.way_id AND wn.version = mv.max_version
  ),
  S3 AS (
    -- Insert into current_way_tags
    INSERT INTO current_way_tags (way_id, k, v)
    SELECT wt.way_id, wt.k, wt.v
    FROM way_tags wt
    JOIN max_versions mv
    ON wt.way_id = mv.way_id AND wt.version = mv.max_version
  )
  SELECT TRUE;

  )");
}

void copy_relations_to_current_relations(Transaction_Manager &m) {

  m.exec(R"(

    WITH max_versions AS (
        SELECT relation_id, MAX(version) AS max_version
        FROM relations
        GROUP BY relation_id
    ),
    S1 AS (
      -- Insert into current_relations
      INSERT INTO current_relations (id, changeset_id, "timestamp", visible, version)
      SELECT r.relation_id, r.changeset_id, r."timestamp", r.visible, r.version
      FROM relations r
      JOIN max_versions mv ON r.relation_id = mv.relation_id AND r.version = mv.max_version
    ),
    S2 AS (
      -- Insert into current_relation_members
      INSERT INTO current_relation_members (relation_id, member_type, member_id, member_role, sequence_id)
      SELECT rm.relation_id, rm.member_type, rm.member_id, rm.member_role, rm.sequence_id
      FROM relation_members rm
      JOIN max_versions mv
      ON rm.relation_id = mv.relation_id AND rm.version = mv.max_version
    ),
    S3 AS (
      -- Insert into current_relation_tags
      INSERT INTO current_relation_tags (relation_id, k, v)
      SELECT rt.relation_id, rt.k, rt.v
      FROM relation_tags rt
      JOIN max_versions mv
      ON rt.relation_id = mv.relation_id AND rt.version = mv.max_version
    )
    SELECT TRUE;
  )");
}

void update_users(Transaction_Manager &m) {

  m.exec(R"(

    update users
      set data_public = true,
	  creation_time = res.created_at,
	  tou_agreed = res.created_at,
	  status = 'active',
	  changesets_count = res.cs_count,
	  terms_agreed = res.created_at,
	  terms_seen = true
      from (
	  select user_id,
		count(*) as cs_count,
		min(created_at) as created_at
	  from changesets
	  group by user_id
      ) as res
      where users.id = res.user_id;
     )");
}

void update_changesets(Transaction_Manager &m) {

  m.exec(R"(

    update changesets
      set num_changes = res.num_changes
      from (
	select changeset_id, sum(changes) as num_changes from (
		    select changeset_id, count(*) as changes from nodes group by changeset_id
		    union all
		    select changeset_id, count(*) as changes from ways group by changeset_id
		    union all
		    select changeset_id, count(*) as changes from relations group by changeset_id)
	as sub
	group by changeset_id)
      as res
    where changesets.id = res.changeset_id;

     )");
}

void create_users(
    Transaction_Manager &m,
    const std::map<osm_user_id_t, std::string> &user_display_names) {

  m.prepare("create_user",
            "INSERT INTO users (id, email, pass_crypt, creation_time, "
            "display_name, data_public) VALUES ($1, $2, $3, $4, $5, $6)");

  for (const auto &[id, name] : user_display_names) {
    auto rc =
        m.exec_prepared("create_user", id, fmt::format("user_{}@demo.abc", id),
                        "", "2025-01-01T00:00:00Z", name, true);
  }
}

void create_user_roles(Transaction_Manager &m, const user_roles_t &user_roles) {

  if (user_roles.empty())
    return;

  m.prepare("user_roles_insert",
            R"(
        WITH tmp_user_role(id, user_id, role, created_at, updated_at, granter_id) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS integer[]),
       CAST($2 AS bigint[]),
       CAST($3 AS user_role_enum[]),
       CAST($4 AS timestamp without time zone[]),
       CAST($5 AS timestamp without time zone[]),
       CAST($6 AS bigint[])
     )
        )
        INSERT INTO user_roles (id, user_id, role, created_at, updated_at, granter_id)
        SELECT * FROM tmp_user_role
    )");

  std::vector<int64_t> ids;
  std::vector<osm_user_id_t> user_ids;
  std::vector<std::string> roles;
  std::vector<std::string> created_ats;
  std::vector<std::string> updated_ats;
  std::vector<osm_user_id_t> granter_ids;

  int id_counter = 1;
  auto current_time = "now()";

  for (const auto &[user_id, rs] : user_roles) {
    using enum osm_user_role_t;

    for (const auto &role : rs) {
      ids.emplace_back(id_counter++);
      user_ids.emplace_back(user_id);
      switch (role) {
      case administrator:
        roles.emplace_back("administrator");
        break;
      case importer:
        roles.emplace_back("importer");
        break;
      case moderator:
        roles.emplace_back("moderator");
        break;
      }
      created_ats.emplace_back(current_time);
      updated_ats.emplace_back(current_time);
      granter_ids.emplace_back(1);
    }
  }

  auto r = m.exec_prepared("user_roles_insert", ids, user_ids, roles,
                           created_ats, updated_ats, granter_ids);
}

void create_oauth2_tokens(Transaction_Manager &m,
                          const oauth2_tokens &oauth2_tokens) {
  if (oauth2_tokens.empty())
    return;

  m.exec(R"(

      INSERT INTO oauth_applications (id, owner_type, owner_id, name, uid, secret,
      redirect_uri, scopes, confidential, created_at, updated_at) VALUES (3, 'User',
      1, 'App 1', 'dHKmvGkmuoMjqhCNmTJkf-EcnA61Up34O1vOHwTSvU8',
      '965136b8fb8d00e2faa2faaaed99c0ec10225518d0c8d9fb1d2af701e87eb68c',
                  'http://demo.localhost:3000', 'write_api read_gpx', false,
      '2021-04-12 17:53:30', '2021-04-12 17:53:30');
  )");

  m.prepare("oauth2_tokens_insert",
            R"(
        WITH tmp_token(id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS integer[]),
       CAST($2 AS integer[]),
       CAST($3 AS integer[]),
       CAST($4 AS text[]),
       CAST($5 AS text[]),
       CAST($6 AS integer[]),
       CAST($7 AS timestamp without time zone[]),
       CAST($8 AS timestamp without time zone[]),
       CAST($9 AS text[])
     )
        )
        INSERT INTO oauth_access_tokens (id, resource_owner_id, application_id, token, refresh_token, expires_in, revoked_at, created_at, scopes)
        SELECT * FROM tmp_token
    )");

  std::vector<int64_t> ids;
  std::vector<int64_t> resource_owner_ids;
  std::vector<int64_t> application_ids;
  std::vector<std::string> tokens;
  std::vector<std::string> refresh_tokens;
  std::vector<int64_t> expires_ins;
  std::vector<std::string> revoked_ats;
  std::vector<std::string> created_ats;
  std::vector<std::string> scopes;

  int64_t counter = 1;

  for (const auto &[token, detail] : oauth2_tokens) {
    ids.emplace_back(counter++);
    resource_owner_ids.emplace_back(detail.user_id);
    application_ids.emplace_back(3);
    tokens.emplace_back(token);
    refresh_tokens.emplace_back("");
    expires_ins.emplace_back(86400);
    revoked_ats.emplace_back(detail.revoked ? "now()" : "");
    created_ats.emplace_back("now()");
    scopes.emplace_back("");
  }

  auto r = m.exec_prepared("oauth2_tokens_insert", ids, resource_owner_ids,
                           application_ids, tokens, refresh_tokens, expires_ins,
                           revoked_ats, created_ats, scopes);
}

void create_changesets(
    Transaction_Manager &m,
    const decltype(xmlparser::database::m_changesets) changesets) {

  if (!changesets.empty()) {

    {
      m.prepare("changesets_insert",
                R"(
	      WITH tmp_changeset(id, user_id, created_at, closed_at, min_lat, max_lat, min_lon, max_lon, num_changes) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS bigint[]),
			 CAST($3 AS timestamp without time zone[]),
			 CAST($4 AS timestamp without time zone[]),
			 CAST($5 AS integer[]),
			 CAST($6 AS integer[]),
			 CAST($7 AS integer[]),
			 CAST($8 AS integer[]),
			 CAST($9 AS integer[])
		 )
	      )
	      INSERT INTO changesets (id, user_id, created_at, closed_at, min_lat, max_lat, min_lon, max_lon, num_changes)
	      SELECT * FROM tmp_changeset
	  )");

      std::vector<osm_changeset_id_t> ids;
      std::vector<osm_user_id_t> user_ids;
      std::vector<std::string> created_ats;
      std::vector<std::string> closed_ats;
      std::vector<int64_t> min_lats;
      std::vector<int64_t> max_lats;
      std::vector<int64_t> min_lons;
      std::vector<int64_t> max_lons;
      std::vector<int64_t> num_changes;

      for (const auto &[id, changeset] : changesets) {
        if (!(changeset.m_info.bounding_box))
          continue;

        ids.emplace_back(id);
        user_ids.emplace_back(changeset.m_info.uid.value_or(0));
        created_ats.emplace_back(changeset.m_info.created_at);
        closed_ats.emplace_back(changeset.m_info.closed_at);
        min_lats.emplace_back(
            static_cast<int64_t>(round(changeset.m_info.bounding_box->minlat *
                                       global_settings::get_scale())));
        max_lats.emplace_back(
            static_cast<int64_t>(round(changeset.m_info.bounding_box->maxlat *
                                       global_settings::get_scale())));
        min_lons.emplace_back(
            static_cast<int64_t>(round(changeset.m_info.bounding_box->minlon *
                                       global_settings::get_scale())));
        max_lons.emplace_back(
            static_cast<int64_t>(round(changeset.m_info.bounding_box->maxlon *
                                       global_settings::get_scale())));
        num_changes.emplace_back(changeset.m_info.num_changes);
      }

      auto r = m.exec_prepared("changesets_insert", ids, user_ids, created_ats,
                               closed_ats, min_lats, max_lats, min_lons,
                               max_lons, num_changes);
    }

    {

      m.prepare("changesets_insert_nobbox",
                R"(
	      WITH tmp_changeset(id, user_id, created_at, closed_at, num_changes) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS bigint[]),
			 CAST($3 AS timestamp without time zone[]),
			 CAST($4 AS timestamp without time zone[]),
			 CAST($5 AS integer[])
		 )
	      )
	      INSERT INTO changesets (id, user_id, created_at, closed_at, num_changes)
	      SELECT * FROM tmp_changeset
	  )");

      std::vector<osm_changeset_id_t> ids;
      std::vector<osm_user_id_t> user_ids;
      std::vector<std::string> created_ats;
      std::vector<std::string> closed_ats;
      std::vector<int64_t> num_changes;

      for (const auto &[id, changeset] : changesets) {
        if ((changeset.m_info.bounding_box))
          continue;

        ids.emplace_back(id);
        user_ids.emplace_back(changeset.m_info.uid.value_or(0));
        created_ats.emplace_back(changeset.m_info.created_at);
        closed_ats.emplace_back(changeset.m_info.closed_at);
        num_changes.emplace_back(changeset.m_info.num_changes);
      }

      auto r = m.exec_prepared("changesets_insert_nobbox", ids, user_ids,
                               created_ats, closed_ats, num_changes);
    }
  }
}

void create_changeset_tags(
    Transaction_Manager &m,
    const decltype(xmlparser::database::m_changesets) changesets) {

  if (changesets.empty())
    return;

  m.prepare("changeset_tags_insert",
            R"(
        WITH tmp_tag(changeset_id, k, v) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS bigint[]),
       CAST($2 AS character varying[]),
       CAST($3 AS character varying[])
     )
        )
        INSERT INTO changeset_tags (changeset_id, k, v)
        SELECT * FROM tmp_tag
    )");

  std::vector<osm_changeset_id_t> changeset_ids;
  std::vector<std::string> keys;
  std::vector<std::string> values;

  for (const auto &[changeset_id, changeset] : changesets) {
    for (const auto &[key, value] : changeset.m_tags) {
      changeset_ids.emplace_back(changeset_id);
      keys.emplace_back(key);
      values.emplace_back(value);
    }
  }

  auto r =
      m.exec_prepared("changeset_tags_insert", changeset_ids, keys, values);
}

void create_changeset_discussions(
    Transaction_Manager &m,
    const decltype(xmlparser::database::m_changesets) changesets) {

  if (changesets.empty())
    return;

  m.prepare("changeset_comments_insert",
            R"(
        WITH tmp_comment(id, changeset_id, author_id, body, created_at, visible) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS integer[]),
       CAST($2 AS bigint[]),
       CAST($3 AS bigint[]),
       CAST($4 AS text[]),
       CAST($5 AS timestamp without time zone[]),
       CAST($6 AS boolean[])
     )
        )
        INSERT INTO changeset_comments (id, changeset_id, author_id, body, created_at, visible)
        SELECT * FROM tmp_comment
    )");

  std::vector<int64_t> ids;
  std::vector<osm_changeset_id_t> changeset_ids;
  std::vector<osm_user_id_t> author_ids;
  std::vector<std::string> bodies;
  std::vector<std::string> created_ats;
  std::vector<std::string> visibles;

  for (const auto &[changeset_id, changeset] : changesets) {
    for (const auto &comment : changeset.m_comments) {
      ids.emplace_back(comment.id);
      changeset_ids.emplace_back(changeset_id);
      author_ids.emplace_back(comment.author_id);
      bodies.emplace_back(comment.body);
      created_ats.emplace_back(comment.created_at);
      visibles.emplace_back("true");
    }
  }

  auto r = m.exec_prepared("changeset_comments_insert", ids, changeset_ids,
                           author_ids, bodies, created_ats, visibles);
}

void changeset_tags_insert(Transaction_Manager &m, osm_changeset_id_t changeset,
                           std::map<std::string, std::string> &tags) {
  if (tags.empty())
    return;

  m.prepare("changeset_tags_insert",
            R"(

	      WITH tmp_tag(changeset_id, k, v) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS character varying[]),
			 CAST($3 AS character varying[])
		 )
	      )
	      INSERT INTO changeset_tags (changeset_id, k, v)
	      SELECT * FROM tmp_tag
	  )");

  std::vector<osm_changeset_id_t> cs;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  for (const auto &[key, value] : tags) {
    cs.emplace_back(changeset);
    ks.emplace_back(escape(key));
    vs.emplace_back(escape(value));
  }

  auto r = m.exec_prepared("changeset_tags_insert", cs, ks, vs);
}

void nodes_insert(Transaction_Manager &m,
                  const decltype(xmlparser::database::m_nodes) &nodes) {
  if (nodes.empty())
    return;

  m.prepare("nodes_insert",
            R"(
	      WITH tmp_node(node_id, latitude, longitude, changeset_id, visible, "timestamp", tile, version) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS integer[]),
			 CAST($3 AS integer[]),
			 CAST($4 AS bigint[]),
			 CAST($5 AS bool[]),
			 CAST($6 AS timestamp without time zone[]),
			 CAST($7 AS bigint[]),
			 CAST($8 AS bigint[])
		 )
	      )
	      INSERT INTO nodes (node_id, latitude, longitude, changeset_id, visible, "timestamp", tile, version)
	      SELECT * FROM tmp_node
	  )");

  std::vector<osm_nwr_id_t> ids;
  std::vector<int64_t> latitudes;
  std::vector<int64_t> longitudes;
  std::vector<osm_changeset_id_t> changeset_ids;
  std::vector<std::string> visibles;
  std::vector<std::string> timestamps;
  std::vector<uint64_t> tiles;
  std::vector<osm_version_t> versions;

  for (const auto &[id_version, node] : nodes) {
    ids.emplace_back(id_version.id);
    latitudes.emplace_back(
        static_cast<int64_t>(round(node.m_lat * global_settings::get_scale())));
    longitudes.emplace_back(
        static_cast<int64_t>(round(node.m_lon * global_settings::get_scale())));
    changeset_ids.emplace_back(node.m_info.changeset);
    visibles.emplace_back(node.m_info.visible ? "true" : "false");
    timestamps.emplace_back(node.m_info.timestamp);
    tiles.emplace_back(xy2tile(lon2x(node.m_lon), lat2y(node.m_lat)));
    versions.emplace_back(id_version.version.value_or(1));
  }

  auto r =
      m.exec_prepared("nodes_insert", ids, latitudes, longitudes, changeset_ids,
                      visibles, timestamps, tiles, versions);
}

void ways_insert(Transaction_Manager &m,
                 const decltype(xmlparser::database::m_ways) &ways) {

  if (ways.empty())
    return;

  m.prepare("ways_insert",
            R"(
				WITH tmp_way(way_id, changeset_id, "timestamp", visible, version) AS (
			SELECT * FROM
			UNNEST( CAST($1 AS bigint[]),
				CAST($2 AS bigint[]),
				CAST($3 AS timestamp without time zone[]),
				CAST($4 AS bool[]),
				CAST($5 AS bigint[])
			)
				)
				INSERT INTO ways (way_id, changeset_id, "timestamp", visible, version)
				SELECT * FROM tmp_way
		)");

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_changeset_id_t> changeset_ids;
  std::vector<std::string> timestamps;
  std::vector<std::string> visibles;
  std::vector<osm_version_t> versions;

  for (const auto &[id_version, way] : ways) {
    ids.emplace_back(id_version.id);
    changeset_ids.emplace_back(way.m_info.changeset);
    timestamps.emplace_back(way.m_info.timestamp);
    visibles.emplace_back(way.m_info.visible ? "true" : "false");
    versions.emplace_back(id_version.version.value_or(1));
  }

  auto r = m.exec_prepared("ways_insert", ids, changeset_ids, timestamps,
                           visibles, versions);
}

void relations_insert(Transaction_Manager &m,
                      const decltype(xmlparser::database::m_relations) &rels) {

  if (rels.empty())
    return;

  m.prepare("relations_insert",
            R"(
		WITH tmp_relation(relation_id, changeset_id, "timestamp", visible, version) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS bigint[]),
			 CAST($3 AS timestamp without time zone[]),
			 CAST($4 AS bool[]),
			 CAST($5 AS bigint[])
		 )
		)
		INSERT INTO relations (relation_id, changeset_id, "timestamp", visible, version)
		SELECT * FROM tmp_relation
		)");

  std::vector<osm_nwr_id_t> ids;
  std::vector<osm_changeset_id_t> changeset_ids;
  std::vector<std::string> timestamps;
  std::vector<std::string> visibles;
  std::vector<osm_version_t> versions;

  for (const auto &[id_version, relation] : rels) {
    ids.emplace_back(id_version.id);
    changeset_ids.emplace_back(relation.m_info.changeset);
    timestamps.emplace_back(relation.m_info.timestamp);
    visibles.emplace_back(relation.m_info.visible ? "true" : "false");
    versions.emplace_back(id_version.version.value_or(1));
  }

  auto r = m.exec_prepared("relations_insert", ids, changeset_ids, timestamps,
                           visibles, versions);
}

void way_nodes_insert(Transaction_Manager &m,
                      const decltype(xmlparser::database::m_ways) &ways) {

  if (ways.empty())
    return;

  m.prepare("way_nodes_insert",
            R"(
		WITH tmp_way_node(way_id, node_id, version, sequence_id) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS bigint[]),
       CAST($3 AS bigint[]),
			 CAST($4 AS bigint[])
		 )
		)
		INSERT INTO way_nodes (way_id, node_id, version, sequence_id)
		SELECT * FROM tmp_way_node
		)");

  std::vector<osm_nwr_id_t> way_ids;
  std::vector<osm_nwr_id_t> node_ids;
  std::vector<int64_t> versions;
  std::vector<int64_t> sequence_ids;

  for (const auto &[id_version, way] : ways) {
    int64_t sequence_id = 1;
    for (const auto &node_id : way.m_nodes) {
      way_ids.emplace_back(id_version.id);
      node_ids.emplace_back(node_id);
      versions.emplace_back(id_version.version.value_or(1));
      sequence_ids.emplace_back(sequence_id++);
    }
  }

  auto r = m.exec_prepared("way_nodes_insert", way_ids, node_ids, versions,
                           sequence_ids);
}

void node_tags_insert(Transaction_Manager &m,
                      const decltype(xmlparser::database::m_nodes) &nodes) {
  if (nodes.empty())
    return;

  m.prepare("node_tags_insert",
            R"(

	      WITH tmp_tag(node_id, version, k, v) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
       CAST($2 AS bigint[]),
			 CAST($3 AS character varying[]),
			 CAST($4 AS character varying[])
		 )
	      )
	      INSERT INTO node_tags (node_id, version, k, v)
	      SELECT * FROM tmp_tag
	  )");

  std::vector<osm_nwr_id_t> ns;
  std::vector<int64_t> versions;
  std::vector<std::string> ks;
  std::vector<std::string> vs;

  for (const auto &node : nodes) {
    for (const auto &[key, value] : node.second.m_tags) {
      ns.emplace_back(node.first.id);
      versions.emplace_back(node.first.version.value_or(1));
      ks.emplace_back(escape(key));
      vs.emplace_back(escape(value));
    }
  }

  auto r = m.exec_prepared("node_tags_insert", ns, versions, ks, vs);
}

template <typename T>
void create_redactions(Transaction_Manager &m, osm_user_id_t uid,
                       const T &objs) {

  if (objs.empty())
    return;

  m.prepare("redactions_upsert",
            R"(
        WITH tmp_redaction(id, title, description, created_at, updated_at, user_id) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS integer[]),
       CAST($2 AS character varying[]),
       CAST($3 AS text[]),
       CAST($4 AS timestamp without time zone[]),
       CAST($5 AS timestamp without time zone[]),
       CAST($6 AS bigint[])
     )
        )
        INSERT INTO redactions (id, title, description, created_at, updated_at, user_id)
        SELECT * FROM tmp_redaction
        ON CONFLICT (id) DO UPDATE SET
          title = EXCLUDED.title,
          description = EXCLUDED.description,
          created_at = EXCLUDED.created_at,
          updated_at = EXCLUDED.updated_at,
          user_id = EXCLUDED.user_id
    )");

  std::vector<int64_t> ids;
  std::vector<std::string> titles;
  std::vector<std::string> descriptions;
  std::vector<std::string> created_ats;
  std::vector<std::string> updated_ats;
  std::vector<int64_t> user_ids;

  for (const auto &[id_version, elem] : objs) {
    if (elem.m_info.redaction) {
      ids.emplace_back(*elem.m_info.redaction);
      titles.emplace_back("Fake Title");
      descriptions.emplace_back("Fake Description");
      created_ats.emplace_back("2025-01-01T00:00:00Z");
      updated_ats.emplace_back("2025-01-01T00:00:00Z");
      user_ids.emplace_back(uid);
    }
  }

  auto r = m.exec_prepared("redactions_upsert", ids, titles, descriptions,
                           created_ats, updated_ats, user_ids);
}

void node_redactions(Transaction_Manager &m,
                     const decltype(xmlparser::database::m_nodes) &nodes) {

  m.prepare("node_redactions_update",
            R"(
        WITH tmp_node_redaction(node_id, version, redaction_id) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS bigint[]),
       CAST($2 AS bigint[]),
       CAST($3 AS bigint[])
     )
        )
        UPDATE nodes
        SET redaction_id = tmp_node_redaction.redaction_id
        FROM tmp_node_redaction
        WHERE nodes.node_id = tmp_node_redaction.node_id
        AND nodes.version = tmp_node_redaction.version
    )");

  std::vector<osm_nwr_id_t> node_ids;
  std::vector<osm_version_t> versions;
  std::vector<osm_redaction_id_t> redaction_ids;

  for (const auto &[id_version, node] : nodes) {
    if (node.m_info.redaction && node.m_info.version) {
      node_ids.emplace_back(id_version.id);
      versions.emplace_back(*id_version.version);
      redaction_ids.emplace_back(*node.m_info.redaction);
    }
  }

  auto r = m.exec_prepared("node_redactions_update", node_ids, versions,
                           redaction_ids);
}

void way_redactions(Transaction_Manager &m,
                    const decltype(xmlparser::database::m_ways) &ways) {

  m.prepare("way_redactions_update",
            R"(
        WITH tmp_way_redaction(way_id, version, redaction_id) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS bigint[]),
       CAST($2 AS bigint[]),
       CAST($3 AS bigint[])
     )
        )
        UPDATE ways
        SET redaction_id = tmp_way_redaction.redaction_id
        FROM tmp_way_redaction
        WHERE ways.way_id = tmp_way_redaction.way_id
        AND ways.version = tmp_way_redaction.version
    )");

  std::vector<osm_nwr_id_t> way_ids;
  std::vector<osm_version_t> versions;
  std::vector<osm_redaction_id_t> redaction_ids;

  for (const auto &[id_version, way] : ways) {
    if (way.m_info.redaction && id_version.version) {
      way_ids.emplace_back(id_version.id);
      versions.emplace_back(*id_version.version);
      redaction_ids.emplace_back(*way.m_info.redaction);
    }
  }

  auto r = m.exec_prepared("way_redactions_update", way_ids, versions,
                           redaction_ids);
}

void relation_redactions(
    Transaction_Manager &m,
    const decltype(xmlparser::database::m_relations) &relations) {

  m.prepare("relation_redactions_update",
            R"(
        WITH tmp_relation_redaction(relation_id, version, redaction_id) AS (
     SELECT * FROM
     UNNEST( CAST($1 AS bigint[]),
       CAST($2 AS bigint[]),
       CAST($3 AS bigint[])
     )
        )
        UPDATE relations
        SET redaction_id = tmp_relation_redaction.redaction_id
        FROM tmp_relation_redaction
        WHERE relations.relation_id = tmp_relation_redaction.relation_id
        AND relations.version = tmp_relation_redaction.version
    )");

  std::vector<osm_nwr_id_t> relation_ids;
  std::vector<osm_version_t> versions;
  std::vector<osm_redaction_id_t> redaction_ids;

  for (const auto &[id_version, relation] : relations) {
    if (relation.m_info.redaction && id_version.version) {
      relation_ids.emplace_back(id_version.id);
      versions.emplace_back(*id_version.version);
      redaction_ids.emplace_back(*relation.m_info.redaction);
    }
  }

  auto r = m.exec_prepared("relation_redactions_update", relation_ids, versions,
                           redaction_ids);
}

void way_tags_insert(Transaction_Manager &m,
                     const decltype(xmlparser::database::m_ways) &ways) {

  if (ways.empty())
    return;

  m.prepare("way_tags_insert",
            R"(

	      WITH tmp_tag(way_id, k, v, version) AS (
		 SELECT * FROM
		 UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS character varying[]),
			 CAST($3 AS character varying[]),
       CAST($4 AS bigint[])
		 )
	      )
	      INSERT INTO way_tags (way_id, k, v, version)
	      SELECT * FROM tmp_tag
	  )");

  std::vector<osm_nwr_id_t> ws;
  std::vector<std::string> ks;
  std::vector<std::string> vs;
  std::vector<int64_t> versions;

  for (const auto &way : ways) {
    for (const auto &[key, value] : way.second.m_tags) {
      ws.emplace_back(way.first.id);
      ks.emplace_back(escape(key));
      vs.emplace_back(escape(value));
      versions.emplace_back(way.first.version.value_or(1));
    }
  }

  auto r = m.exec_prepared("way_tags_insert", ws, ks, vs, versions);
}

void relation_tags_insert(
    Transaction_Manager &m,
    const decltype(xmlparser::database::m_relations) &relations) {
  if (relations.empty())
    return;

  m.prepare("relation_tags_insert",
            R"(

	      WITH tmp_tag(relation_id, k, v, version) AS (
		 SELECT * FROM
		UNNEST( CAST($1 AS bigint[]),
			 CAST($2 AS character varying[]),
			 CAST($3 AS character varying[]),
			 CAST($4 AS bigint[])
		 )
	      )
	      INSERT INTO relation_tags (relation_id, k, v, version)
	      SELECT * FROM tmp_tag
	  )");

  std::vector<osm_nwr_id_t> rs;
  std::vector<std::string> ks;
  std::vector<std::string> vs;
  std::vector<int64_t> versions;

  for (const auto &relation : relations) {
    for (const auto &[key, value] : relation.second.m_tags) {
      rs.emplace_back(relation.first.id);
      ks.emplace_back(escape(key));
      vs.emplace_back(escape(value));
      versions.emplace_back(relation.first.version.value_or(1));
    }
  }

  auto r = m.exec_prepared("relation_tags_insert", rs, ks, vs, versions);
}

std::string convert_element_type_name(element_type elt) noexcept {

  using enum element_type;
  switch (elt) {
  case node:
    return "Node";
  case way:
    return "Way";
  case relation:
    return "Relation";
  }
  return "";
}

void relation_members_insert(
    Transaction_Manager &m,
    const decltype(xmlparser::database::m_relations) &relations) {
  if (relations.empty())
    return;

  m.prepare("relation_members_insert",
            R"(

	WITH tmp_relation_member(relation_id, member_type, member_id, member_role, version, sequence_id) AS (
	SELECT * FROM
	UNNEST( CAST($1 AS bigint[]),
		CAST($2 AS nwr_enum[]),
		CAST($3 AS bigint[]),
		CAST($4 AS character varying[]),
    CAST($5 AS bigint[]),
		CAST($6 AS integer[])
	)
	)
	INSERT INTO relation_members (relation_id, member_type, member_id, member_role, version, sequence_id)
	SELECT * FROM tmp_relation_member
	)");

  std::vector<osm_nwr_id_t> relation_ids;
  std::vector<std::string> member_types;
  std::vector<osm_nwr_id_t> member_ids;
  std::vector<std::string> member_roles;
  std::vector<int64_t> versions;
  std::vector<int64_t> sequence_ids;

  for (const auto &[id_version, relation] : relations) {
    int sequence_id = 1;
    for (const auto &member : relation.m_members) {
      relation_ids.emplace_back(id_version.id);
      member_types.emplace_back(convert_element_type_name(member.type));
      member_ids.emplace_back(member.ref);
      member_roles.emplace_back(escape(member.role));
      versions.emplace_back(id_version.version.value_or(1));
      sequence_ids.emplace_back(sequence_id++);
    }
  }

  auto r =
      m.exec_prepared("relation_members_insert", relation_ids, member_types,
                      member_ids, member_roles, versions, sequence_ids);
}

void populate_database(Transaction_Manager &m, const xmlparser::database &db,
                       const user_roles_t &user_roles,
                       const oauth2_tokens &oauth2_tokens) {

  std::map<osm_user_id_t, std::string> user_display_names;
  std::map<osm_changeset_id_t, int> changeset_object_counts;
  std::map<osm_changeset_id_t, osm_user_id_t> changeset_uid;
  std::set<osm_redaction_id_t> redaction_ids;

  auto process_info = [&](const auto &info) {
    user_display_names[info.uid.value_or(0)] = info.display_name.value_or("");
    changeset_object_counts[info.changeset]++;
    changeset_uid[info.changeset] = info.uid.value_or(0);
    if (info.redaction)
      redaction_ids.insert(*info.redaction);
  };

  for (const auto &[id, node] : db.m_nodes) {
    process_info(node.m_info);
  }

  for (const auto &[id, way] : db.m_ways) {
    process_info(way.m_info);
  }

  for (const auto &[id, relation] : db.m_relations) {
    process_info(relation.m_info);
  }

  for (const auto &[id, changeset] : db.m_changesets) {
    user_display_names[changeset.m_info.uid.value_or(0)] =
        changeset.m_info.display_name.value_or("");
  }

  for (const auto &[user_id, _] : user_roles) {
    if (!user_display_names.contains(user_id)) {
      user_display_names[user_id] = "";
    }
  }

  // Create users
  create_users(m, user_display_names);
  create_user_roles(m, user_roles);
  create_oauth2_tokens(m, oauth2_tokens);

  // Update redactions table
  create_redactions(m, user_display_names.begin()->first, db.m_nodes);
  create_redactions(m, user_display_names.begin()->first, db.m_ways);
  create_redactions(m, user_display_names.begin()->first, db.m_relations);

  // Create changesets
  auto changesets = db.m_changesets;

  // create dummy changesets if no changesets available
  if (changesets.empty()) {
    for (const auto &cs : changeset_object_counts) {
      xmlparser::changeset c;
      c.m_info.created_at = "2025-01-01T00:00:00Z";
      c.m_info.closed_at = "2025-01-01T01:00:00Z";
      c.m_info.uid = changeset_uid[cs.first];
      c.m_info.num_changes = cs.second;
      changesets[cs.first] = c;
    }
  }
  create_changesets(m, changesets);
  create_changeset_tags(m, changesets);
  create_changeset_discussions(m, changesets);

  // Insert nodes
  nodes_insert(m, db.m_nodes);
  node_tags_insert(m, db.m_nodes);
  node_redactions(m, db.m_nodes);

  // Insert ways
  ways_insert(m, db.m_ways);
  way_tags_insert(m, db.m_ways);
  way_nodes_insert(m, db.m_ways);
  way_redactions(m, db.m_ways);

  // Insert relations
  relations_insert(m, db.m_relations);
  relation_tags_insert(m, db.m_relations);
  relation_members_insert(m, db.m_relations);
  relation_redactions(m, db.m_relations);

  // Copy latest object version to current table
  copy_nodes_to_current_nodes(m);
  copy_ways_to_current_ways(m);
  copy_relations_to_current_relations(m);

  // Update stats for user and changesets
  update_users(m);
  update_changesets(m);
}
