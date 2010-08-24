#ifndef OSM_HELPERS_HPP
#define OSM_HELPERS_HPP

#include <pqxx/pqxx>
#include "output_formatter.hpp"

namespace osm_helpers {

/* output writing functions */

/// write the nodes referenced in the tmp_nodes table to the output formatter.
void write_tmp_nodes(pqxx::work &w, output_formatter &formatter, int num_nodes);

/// write the ways referenced in the tmp_ways table to the output formatter.
void write_tmp_ways(pqxx::work &w, output_formatter &formatter, int num_ways);

/// write the relations referenced in the tmp_relations table to the output formatter.
void write_tmp_relations(pqxx::work &w, output_formatter &formatter, int num_relations);

/* information functions -- should be pretty obvious what they do */

int num_nodes(pqxx::work &x);
int num_ways(pqxx::work &x);
int num_relations(pqxx::work &x);

/* functions to update the tmp_* tables. */

/// given a bounding box, populate the tmp_nodes table up to a limit of max_nodes
void create_tmp_nodes_from_bbox(pqxx::work &w, const bbox &bounds, int max_nodes);

/// creates tmp_ways from nodes via the way_nodes table
void create_tmp_ways_from_nodes(pqxx::work &w);

/// create tmp_relations from the tmp_ways table
void create_tmp_relations_from_ways(pqxx::work &w);

/// update tmp_nodes to include way nodes from tmp_ways
void insert_tmp_nodes_from_way_nodes(pqxx::work &w);

/// update tmp_relations to include tmp_nodes 
void insert_tmp_relations_from_nodes(pqxx::work &w);

/// update tmp_relations to include tmp_nodes via relation_members
void insert_tmp_relations_from_way_nodes(pqxx::work &w);

/// update tmp_relations to include other relations
void insert_tmp_relations_from_relations(pqxx::work &w);

}

#endif /* OSM_HELPERS_HPP */
