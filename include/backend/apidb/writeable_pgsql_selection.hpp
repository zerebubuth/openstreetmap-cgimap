#ifndef WRITEABLE_PGSQL_SELECTION_HPP
#define WRITEABLE_PGSQL_SELECTION_HPP

#include "data_selection.hpp"
#include <pqxx/pqxx>

/**
 * a selection which operates against a writeable (i.e: non read-only
 * slave) PostgreSQL database, such as the rails_port database or
 * an osmosis imported database.
 */
class writeable_pgsql_selection
	: public data_selection {
public:
	 writeable_pgsql_selection(pqxx::connection &conn);
	 ~writeable_pgsql_selection();

	 void write_nodes(output_formatter &formatter);
	 void write_ways(output_formatter &formatter);
	 void write_relations(output_formatter &formatter);

	 int num_nodes();
	 int num_ways();
	 int num_relations();
	 visibility_t check_node_visibility(osm_id_t id);
	 visibility_t check_way_visibility(osm_id_t id);
	 visibility_t check_relation_visibility(osm_id_t id);

	 void select_nodes(const std::list<osm_id_t> &);
	 void select_ways(const std::list<osm_id_t> &);
	 void select_relations(const std::list<osm_id_t> &);
	 void select_nodes_from_bbox(const bbox &bounds, int max_nodes);
	 void select_nodes_from_relations();
	 void select_ways_from_nodes();
	 void select_ways_from_relations();
	 void select_relations_from_ways();
	 void select_nodes_from_way_nodes();
	 void select_relations_from_nodes();
	 void select_relations_from_relations();
  void select_relations_members_of_relations();

   /**
    * abstracts the creation of transactions for the writeable
    * data selection.
    */
   class factory
      : public data_selection::factory {
   public:
      factory(pqxx::connection &);
      virtual ~factory();
      virtual boost::shared_ptr<data_selection> make_selection();

   private:
      pqxx::connection &m_connection;
   };

private:

   // the transaction in which the selection takes place. although 
   // this *is* read-only, it may create temporary tables.
   pqxx::work w;
};

#endif /* WRITEABLE_PGSQL_SELECTION_HPP */
