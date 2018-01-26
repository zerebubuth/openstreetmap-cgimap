#ifndef OSMCHANGE_HANDLER_HPP
#define OSMCHANGE_HANDLER_HPP


// TODO: Include only abstract base class, no direct references to backend!
#include "cgimap/backend/apidb/changeset_upload/node_updater.hpp"
#include "cgimap/backend/apidb/changeset_upload/relation_updater.hpp"
#include "types.hpp"
#include "util.hpp"
#include "cgimap/backend/apidb/changeset_upload/way_updater.hpp"

#include <osmium/handler.hpp>




class OSMChange_Handler: public osmium::handler::Handler {

public:

	OSMChange_Handler(std::unique_ptr<Node_Updater> _node_updater, std::unique_ptr<Way_Updater> _way_updater,
			std::unique_ptr<Relation_Updater> _relation_updater, osm_changeset_id_t _changeset, osm_user_id_t _uid);

	// checks common to all objects
	void osm_object(const osmium::OSMObject& o) const;

	void node(const osmium::Node& node);

	void way(const osmium::Way& way);

	void relation(const osmium::Relation& relation);

	void finish_processing();

	unsigned int get_num_changes();

	bbox_t get_bbox();

private:

	enum class operation {
		op_create = 1, op_modify = 2, op_delete = 3
	};

	enum class state {
		st_initial,
		st_create_node,
		st_create_way,
		st_create_relation,
		st_modify,
		st_delete_relation,
		st_delete_way,
		st_delete_node,
		st_finished
	};

	operation get_operation(const osmium::OSMObject& o);

	void handle_new_state(state new_state);

	state current_state { state::st_initial };

	std::unique_ptr<Node_Updater> node_updater;
	std::unique_ptr<Way_Updater> way_updater;
	std::unique_ptr<Relation_Updater> relation_updater;

	osm_changeset_id_t m_changeset;
	osm_user_id_t m_uid;
};


#endif
