/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef CGIMAP_BACKEND_APIDB_COMMON_PGSQL_SELECTION_HPP
#define CGIMAP_BACKEND_APIDB_COMMON_PGSQL_SELECTION_HPP

#include <cgimap/output_formatter.hpp>
#include <cgimap/backend/apidb/changeset.hpp>

#include <chrono>
#include <functional>
#include <pqxx/pqxx>
#include <string_view>
#include <string>
#include <vector>

/* these functions take the results of "rolled-up" queries where tags, way
 * nodes and relation members are aggregated per-row.
 */

// extract nodes from the results of the query and write them to the formatter.
// the changeset cache is used to look up user display names.
void extract_nodes(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  std::map<osm_changeset_id_t, changeset> &cc);

// extract ways from the results of the query and write them to the formatter.
// the changeset cache is used to look up user display names.
void extract_ways(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  std::map<osm_changeset_id_t, changeset> &cc);

// extract relations from the results of the query and write them to the
// formatter. the changeset cache is used to look up user display names.
void extract_relations(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  std::map<osm_changeset_id_t, changeset> &cc);

void extract_changesets(
  const pqxx::result &rows, output_formatter &formatter,
  std::map<osm_changeset_id_t, changeset> &cc,
  const std::chrono::system_clock::time_point &now,
  bool include_changeset_discussions);

// parses psql array based on specs given
// https://www.postgresql.org/docs/current/static/arrays.html#ARRAYS-IO
std::vector<std::string> psql_array_to_vector(std::string_view str);
std::vector<std::string> psql_array_to_vector(const pqxx::field& field);

#endif /* CGIMAP_BACKEND_APIDB_COMMON_PGSQL_SELECTION_HPP */
