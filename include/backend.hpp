#include "output_formatter.hpp"

class backend {
public:
	 virtual ~backend();

	 virtual void write_nodes(output_formatter &formatter) = 0;
	 virtual void write_ways(output_formatter &formatter) = 0;
	 virtual void write_relations(output_formatter &formatter) = 0;

	 virtual int num_nodes() = 0;
	 virtual int num_ways() = 0;
	 virtual int num_relations() = 0;

   /// given a bounding box, select nodes within that bbox up to a limit of max_nodes
	 virtual void select_nodes_from_bbox(pqxx::work &w, const bbox &bounds, int max_nodes) = 0;

   /// selects the node members of any already selected relations
	 virtual void select_nodes_from_relations(pqxx::work &w) = 0;
	 
   /// selects all ways that contain selected nodes
	 virtual void select_ways_from_nodes(pqxx::work &w) = 0;
	 
   /// selects all ways that are members of selected relations
	 virtual void select_ways_from_relations(pqxx::work &w) = 0;
	 
   /// select all relations that contain selected ways
	 virtual void select_relations_from_ways(pqxx::work &w) = 0;
	 
   /// select nodes which are used in selected ways
	 virtual void select_nodes_from_way_nodes(pqxx::work &w) = 0;
	 
   /// select relations which include selected nodes 
	 virtual void select_relations_from_nodes(pqxx::work &w) = 0;
	 
   /// update relations to include nodes via relation_members ?!?!?
	 virtual void select_relations_from_way_nodes(pqxx::work &w) = 0;
	 
   /// select relations which include selected relations
	 virtual void select_relations_from_relations(pqxx::work &w) = 0;
	 
};
