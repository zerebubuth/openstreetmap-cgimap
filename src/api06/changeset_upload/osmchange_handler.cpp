
#include "cgimap/logger.hpp"
#include "cgimap/http.hpp"

#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"


#include <boost/format.hpp>



OSMChange_Handler::OSMChange_Handler(std::unique_ptr<Node_Updater> _node_updater, std::unique_ptr<Way_Updater> _way_updater,
		std::unique_ptr<Relation_Updater> _relation_updater, osm_changeset_id_t _changeset, osm_user_id_t _uid) :
		node_updater(std::move(_node_updater)), way_updater(std::move(_way_updater)),
		relation_updater(std::move(_relation_updater)), m_changeset(_changeset), m_uid(_uid)

{
}


// checks common to all objects
void OSMChange_Handler::osm_object(const osmium::OSMObject& o) const {

	if (o.changeset() != m_changeset)
	  throw http::conflict((boost::format("Changeset mismatch: Provided %1% but only %2% is allowed") % o.changeset() % m_changeset).str());

	for (const auto & tag : o.tags())
	{
		if (unicode_strlen(tag.key()) > 255)
			throw http::bad_request("Key has more than 255 unicode characters");

		if (unicode_strlen(tag.value()) > 255)
			throw http::bad_request("Value has more than 255 unicode characters");
	}

}

void OSMChange_Handler::node(const osmium::Node& node) {
	auto op = get_operation(node);

	switch (op) {
	case operation::op_create:
		handle_new_state(state::st_create_node);
		node_updater->add_node(node.location().lat(), node.location().lon(),
				m_changeset, node.id(), std::move(node.tags()));
		break;

	case operation::op_modify:
		handle_new_state(state::st_modify);
		node_updater->modify_node(node.location().lat(),
				node.location().lon(), m_changeset, node.id(),
				node.version(), std::move(node.tags()));
		break;

	case operation::op_delete:
		assert (1 <= node.uid() && node.uid() <= 2);
		// workaround: non-used uid field -> if-used flag
		bool if_unused = (node.uid() == 2 ? true : false);

		handle_new_state(state::st_delete_node);
		node_updater->delete_node(m_changeset, node.id(), node.version(), if_unused);
		break;
	}
}

void OSMChange_Handler::way(const osmium::Way& way) {
	auto op = get_operation(way);

	switch (op) {
	case operation::op_create:
		handle_new_state(state::st_create_way);
		way_updater->add_way(m_changeset, way.id(), way.nodes(),
				way.tags());
		break;

	case operation::op_modify:
		handle_new_state(state::st_modify);
		way_updater->modify_way(m_changeset, way.id(), way.version(),
				way.nodes(), way.tags());
		break;

	case operation::op_delete:
		assert (1 <= way.uid() && way.uid() <= 2);
		// workaround: non-used uid field -> if-used flag
		bool if_unused = (way.uid() == 2 ? true : false);

		handle_new_state(state::st_delete_way);
		way_updater->delete_way(m_changeset, way.id(), way.version(), if_unused);
		break;
	}
}

void OSMChange_Handler::relation(const osmium::Relation& relation) {
	auto op = get_operation(relation);

	for (const auto & m : relation.members())
	{
		if (unicode_strlen(m.role()) > 255)
			throw http::bad_request("Relation Role has more than 255 unicode characters");
	}

	switch (op) {
	case operation::op_create:
		handle_new_state(state::st_create_relation);
		relation_updater->add_relation(m_changeset, relation.id(),
				relation.members(), relation.tags());
		break;

	case operation::op_modify:
		handle_new_state(state::st_modify);
		relation_updater->modify_relation(m_changeset, relation.id(),
				relation.version(), std::move(relation.members()),
				std::move(relation.tags()));
		break;

	case operation::op_delete:
		assert (1 <= relation.uid() && relation.uid() <= 2);
		// workaround: non-used uid field -> if-used flag
		bool if_unused = (relation.uid() == 2 ? true : false);

		handle_new_state(state::st_delete_relation);
		relation_updater->delete_relation(m_changeset, relation.id(),
				relation.version(), if_unused);
		break;
	}
}

void OSMChange_Handler::finish_processing() {
	handle_new_state(state::st_finished);
}

unsigned int OSMChange_Handler::get_num_changes() {
	return (node_updater->get_num_changes() +
			way_updater->get_num_changes() +
			relation_updater->get_num_changes());
}

bbox_t OSMChange_Handler::get_bbox() {
	bbox_t bbox;

	bbox.expand(node_updater->bbox);
	bbox.expand(way_updater->bbox);
	bbox.expand(relation_updater->bbox);

	return bbox;
}

OSMChange_Handler::operation OSMChange_Handler::get_operation(const osmium::OSMObject& o) {

    // As libosmium objects don't have a dedicated field to store an osmchange operation,
	// we set the version to 0 for objects in operation 'created' and values > 0 for
	// all other operations. In addition, delete operation have the visible flag set to false

	return (o.visible() ?
			((o.version() == 0) ?
					operation::op_create : operation::op_modify) :
			operation::op_delete);
}

void OSMChange_Handler::handle_new_state(state new_state) {

	// TODO: add flush_limit to send data to db if too much memory is used for internal buffers
	if (new_state == current_state /* && object_counter < flush_limit */)
		return;

	// process objects in buffer for the current state before doing a transition to the new state
	switch (current_state) {
	case state::st_initial:
		// nothing to do
		break;

	case state::st_create_node:
		node_updater->process_new_nodes();
		break;

	case state::st_create_way:
		way_updater->process_new_ways();
		break;

	case state::st_create_relation:
		relation_updater->process_new_relations();
		break;

	case state::st_modify:
		node_updater->process_modify_nodes();
		way_updater->process_modify_ways();
		relation_updater->process_modify_relations();
		break;

	case state::st_delete_node:
		node_updater->process_delete_nodes();
		break;

	case state::st_delete_way:
		way_updater->process_delete_ways();
		break;

	case state::st_delete_relation:
		relation_updater->process_delete_relations();
		break;

	case state::st_finished:
		// nothing to do
		break;
	}

	// complete transition to new state
	current_state = new_state;
}


