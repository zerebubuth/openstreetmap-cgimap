#ifndef CGIMAP_BACKEND_APIDB_COMMON_PGSQL_SELECTION_HPP
#define CGIMAP_BACKEND_APIDB_COMMON_PGSQL_SELECTION_HPP

#include <cgimap/output_formatter.hpp>
#include <cgimap/backend/apidb/cache.hpp>
#include <cgimap/backend/apidb/changeset.hpp>
#include <pqxx/pqxx>

/* these functions take the results of "rolled-up" queries where tags, way
 * nodes and relation members are aggregated per-row.
 */

// extract nodes from the results of the query and write them to the formatter.
// the changeset cache is used to look up user display names.
void extract_nodes(
  const pqxx::result &rows, output_formatter &formatter,
  cache<osm_changeset_id_t, changeset> &cc);

// extract ways from the results of the query and write them to the formatter.
// the changeset cache is used to look up user display names.
void extract_ways(
  const pqxx::result &rows, output_formatter &formatter,
  cache<osm_changeset_id_t, changeset> &cc);

// extract relations from the results of the query and write them to the
// formatter. the changeset cache is used to look up user display names.
void extract_relations(
  const pqxx::result &rows, output_formatter &formatter,
  cache<osm_changeset_id_t, changeset> &cc);

void extract_changesets(
  const pqxx::result &rows, output_formatter &formatter,
  cache<osm_changeset_id_t, changeset> &cc,
  const boost::posix_time::ptime &now,
  bool include_changeset_discussions);

#endif /* CGIMAP_BACKEND_APIDB_COMMON_PGSQL_SELECTION_HPP */
