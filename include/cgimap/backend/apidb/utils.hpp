/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef CGIMAP_BACKEND_APIDB_UTILS_HPP
#define CGIMAP_BACKEND_APIDB_UTILS_HPP

#include <pqxx/pqxx>

/* checks that the postgres version is sufficient to run cgimap.
 *
 * some queries (e.g: LATERAL join) and functions (multi-parameter unnest) only
 * became available in later versions of postgresql.
 */
void check_postgres_version(pqxx::connection_base &conn);

// parses psql array based on specs given
// https://www.postgresql.org/docs/current/static/arrays.html#ARRAYS-IO
std::vector<std::string> psql_array_to_vector(std::string_view str, int size_hint = 0);
std::vector<std::string> psql_array_to_vector(const pqxx::field& field, int size_hint = 0);

template <typename T>
std::vector<T> psql_array_ids_to_vector(const pqxx::field& field);

template <typename T>
std::vector<T> psql_array_ids_to_vector(std::string_view str);

#endif /* CGIMAP_BACKEND_APIDB_UTILS_HPP */
