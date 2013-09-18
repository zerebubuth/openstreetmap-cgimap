#ifndef READONLY_PGSQL_SELECTION_HPP
#define READONLY_PGSQL_SELECTION_HPP

#include "data_selection.hpp"
#include "backend/apidb/changeset.hpp"
#include "backend/apidb/cache.hpp"
#include <pqxx/pqxx>
#include <boost/program_options.hpp>
#include <set>

/**
 * a selection which operates against a readonly (e.g: replicating
 * slave) PostgreSQL database. This is needed because, currently, 
 * slaves can't create XIDs which would be necessary to create 
 * temporary tables. Instead, the state of the selection must be
 * stored in-memory in cgimap's memory.
 */
class readonly_pgsql_selection
	: public data_selection {
public:
	 readonly_pgsql_selection(pqxx::connection &conn, cache<osm_id_t, changeset> &changeset_cache);
	 ~readonly_pgsql_selection();

	 void write_nodes(output_formatter &formatter);
	 void write_ways(output_formatter &formatter);
	 void write_relations(output_formatter &formatter);

	 visibility_t check_node_visibility(osm_id_t id);
	 visibility_t check_way_visibility(osm_id_t id);
	 visibility_t check_relation_visibility(osm_id_t id);

	 int select_nodes(const std::list<osm_id_t> &);
	 int select_ways(const std::list<osm_id_t> &);
	 int select_relations(const std::list<osm_id_t> &);
	 int select_nodes_from_bbox(const bbox &bounds, int max_nodes);
	 void select_nodes_from_relations();
	 void select_ways_from_nodes();
	 void select_ways_from_relations();
	 void select_relations_from_ways();
	 void select_nodes_from_way_nodes();
	 void select_relations_from_nodes();
	 void select_relations_from_relations();
  void select_relations_members_of_relations();

   /**
    * a factory for the creation of read-only selections, so it
    * can set up prepared statements.
    */
   class factory
      : public data_selection::factory {
   public:
     factory(const boost::program_options::variables_map &);
     virtual ~factory();
     virtual boost::shared_ptr<data_selection> make_selection();

   private:
     pqxx::connection m_connection, m_cache_connection;
     pqxx::nontransaction m_cache_tx;
     cache<osm_id_t, changeset> m_cache;
   };

private:
	 // the transaction in which the selection takes place. this is
	 // fully read-only, and cannot create any temporary tables, 
	 // unlike writeable_pgsql_selection.
	 pqxx::work w;

	 // the set of selected nodes, ways and relations
	 std::set<osm_id_t> sel_nodes, sel_ways, sel_relations;
   cache<osm_id_t, changeset> &cc;
};

#endif /* READONLY_PGSQL_SELECTION_HPP */
