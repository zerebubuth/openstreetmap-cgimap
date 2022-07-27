#ifndef CGIMAP_BACKEND_APIDB_UTILS_HPP
#define CGIMAP_BACKEND_APIDB_UTILS_HPP

#include <pqxx/pqxx>

/* checks that the postgres version is sufficient to run cgimap.
 *
 * some queries (e.g: LATERAL join) and functions (multi-parameter unnest) only
 * became available in later versions of postgresql.
 */
void check_postgres_version(pqxx::connection_base &conn);

#endif /* CGIMAP_BACKEND_APIDB_UTILS_HPP */
