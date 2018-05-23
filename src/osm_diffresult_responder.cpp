#include "cgimap/osm_diffresult_responder.hpp"
#include "cgimap/config.hpp"

#include <map>

using std::list;
using boost::shared_ptr;
namespace pt = boost::posix_time;


element_type as_elem_type(object_type o) {

  switch (o) {

    case object_type::node:
      return element_type_node;
    case object_type::way:
      return element_type_way;
    case object_type::relation:
      return element_type_relation;
  }
}


osm_diffresult_responder::osm_diffresult_responder(mime::type mt)
    : osm_responder(mt) {}

osm_diffresult_responder::~osm_diffresult_responder() {}

void osm_diffresult_responder::write(shared_ptr<output_formatter> formatter,
                                     const std::string &generator,
                                     const pt::ptime &now) {

  // TODO: is it possible that formatter can be null?
  output_formatter &fmt = *formatter;

  try {
    fmt.start_document(generator, "diffResult");


    // For compatibility reasons, diffResult output matches the exact object sequence provided in the osmChange message.
    // OSM API documentation doesn't provide any guarantees wrt the actual sequence. However, some clients might
    // implicitly rely on osmChange entries to be processed in the sequence given.

    // Prepare maps for faster lookup

    // Create
    std::map< std::pair< object_type, osm_nwr_signed_id_t >, OSMChange_Tracking::object_id_mapping_t > map_create_ids;
    for (const auto &id : change_tracking->created_node_ids)
      map_create_ids.insert(std::make_pair(std::make_pair(object_type::node, id.old_id), id));

    for (const auto &id : change_tracking->created_way_ids)
      map_create_ids.insert(std::make_pair(std::make_pair(object_type::way, id.old_id), id));

    for (const auto &id : change_tracking->created_relation_ids)
      map_create_ids.insert(std::make_pair(std::make_pair(object_type::relation, id.old_id), id));


    // Modify
    std::map< std::tuple< object_type, osm_nwr_signed_id_t, osm_version_t>, OSMChange_Tracking::object_id_mapping_t > map_modify_ids;
    for (const auto &id : change_tracking->modified_node_ids)
      map_modify_ids.insert(std::make_pair(std::make_tuple(object_type::node, id.old_id, id.new_version), id));

    for (const auto &id : change_tracking->modified_way_ids)
      map_modify_ids.insert(std::make_pair(std::make_tuple(object_type::way, id.old_id, id.new_version), id));

    for (const auto &id : change_tracking->modified_relation_ids)
      map_modify_ids.insert(std::make_pair(std::make_tuple(object_type::relation, id.old_id, id.new_version), id));

    // Delete (if-unused)
    std::map< std::pair< object_type, osm_nwr_signed_id_t>, OSMChange_Tracking::object_id_mapping_t > map_skip_delete_ids;
    for (const auto &id : change_tracking->skip_deleted_node_ids)
      map_skip_delete_ids.insert(std::make_pair(std::make_pair(object_type::node, id.old_id), id));

    for (const auto &id : change_tracking->skip_deleted_way_ids)
      map_skip_delete_ids.insert(std::make_pair(std::make_pair(object_type::way, id.old_id), id));

    for (const auto &id : change_tracking->skip_deleted_relation_ids)
      map_skip_delete_ids.insert(std::make_pair(std::make_pair(object_type::relation, id.old_id), id));

    // Deleted object ids
    std::set< std::pair< object_type, osm_nwr_id_t> > set_delete_ids;
    for (const auto &id : change_tracking->deleted_node_ids)
      set_delete_ids.insert(std::make_pair(object_type::node, id));

    for (const auto &id : change_tracking->deleted_way_ids)
      set_delete_ids.insert(std::make_pair(object_type::way, id));

    for (const auto &id : change_tracking->deleted_relation_ids)
      set_delete_ids.insert(std::make_pair(object_type::relation, id));


    // Iterate over all elements in the sequence defined in the osmChange message

    for (const auto &item : change_tracking->osmchange_orig_sequence) {

	switch (item.op) {

	  case operation::op_create:

	    {
	      auto it = map_create_ids.find(std::make_pair(item.obj_type, item.orig_id));
	      if (it != map_create_ids.end()) {
		  auto id = it->second;

		      fmt.write_diffresult_create_modify(as_elem_type(item.obj_type), id.old_id,
							 id.new_id, id.new_version);
	      }
	      else
		throw std::runtime_error ("Element in osmChange message was not processed");
	    }
	    break;

	  case operation::op_modify:
	    {
	      auto it = map_modify_ids.find(std::make_tuple(item.obj_type, item.orig_id, item.orig_version + 1));
	      if (it != map_modify_ids.end()) {
		  auto id = it->second;

	              fmt.write_diffresult_create_modify(as_elem_type(item.obj_type), id.old_id,
	                                                 id.new_id, id.new_version);
	      }
	      else
		throw std::runtime_error ("Element in osmChange message was not processed");
	    }
	    break;

	  case operation::op_delete:

	    if (item.if_unused) {

		auto it = map_skip_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		if (it != map_skip_delete_ids.end()) {
		    auto id = it->second;

		    fmt.write_diffresult_create_modify(as_elem_type(item.obj_type), id.old_id,
						       id.new_id, id.new_version);
		}
		else {

		    auto it = set_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		    if (it != set_delete_ids.end()) {

			fmt.write_diffresult_delete(as_elem_type(item.obj_type), item.orig_id);
		    }
		    else {
			throw std::runtime_error ("Element in osmChange message was not processed");
		    }
		}
	    }

	    if (!item.if_unused) {

		auto it = set_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		if (it != set_delete_ids.end()) {

		    fmt.write_diffresult_delete(as_elem_type(item.obj_type), item.orig_id);
		}
		else {
		    throw std::runtime_error ("Element in osmChange message was not processed");
		}

	    }

	    break;

      }
    }

  } catch (const std::exception &e) {
    fmt.error(e);
  }

  fmt.end_document();
}
