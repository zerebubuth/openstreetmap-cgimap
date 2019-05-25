
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

#include <boost/format.hpp>

namespace api06 {

  std::vector<diffresult_t> OSMChange_Tracking::assemble_diffresult() {

    // For compatibility reasons, diffResult output matches the exact object sequence provided in the osmChange message.
    // OSM API documentation doesn't provide any guarantees wrt the actual sequence. However, some clients might
    // implicitly rely on osmChange entries to be processed in the sequence given.

    // Prepare maps for faster lookup

    // Create
    std::map< std::pair< object_type, osm_nwr_signed_id_t >, OSMChange_Tracking::object_id_mapping_t > map_create_ids;
    for (const auto &id : created_node_ids)
      map_create_ids.insert(std::make_pair(std::make_pair(object_type::node, id.old_id), id));

    for (const auto &id : created_way_ids)
      map_create_ids.insert(std::make_pair(std::make_pair(object_type::way, id.old_id), id));

    for (const auto &id : created_relation_ids)
      map_create_ids.insert(std::make_pair(std::make_pair(object_type::relation, id.old_id), id));


    // Modify
    std::map< std::tuple< object_type, osm_nwr_signed_id_t, osm_version_t>, OSMChange_Tracking::object_id_mapping_t > map_modify_ids;
    for (const auto &id : modified_node_ids)
      map_modify_ids.insert(std::make_pair(std::make_tuple(object_type::node, id.old_id, id.new_version), id));

    for (const auto &id : modified_way_ids)
      map_modify_ids.insert(std::make_pair(std::make_tuple(object_type::way, id.old_id, id.new_version), id));

    for (const auto &id : modified_relation_ids)
      map_modify_ids.insert(std::make_pair(std::make_tuple(object_type::relation, id.old_id, id.new_version), id));

    // Delete (if-unused)
    std::map< std::pair< object_type, osm_nwr_signed_id_t>, OSMChange_Tracking::object_id_mapping_t > map_skip_delete_ids;
    for (const auto &id : skip_deleted_node_ids)
      map_skip_delete_ids.insert(std::make_pair(std::make_pair(object_type::node, id.old_id), id));

    for (const auto &id : skip_deleted_way_ids)
      map_skip_delete_ids.insert(std::make_pair(std::make_pair(object_type::way, id.old_id), id));

    for (const auto &id : skip_deleted_relation_ids)
      map_skip_delete_ids.insert(std::make_pair(std::make_pair(object_type::relation, id.old_id), id));

    // Deleted object ids
    std::set< std::pair< object_type, osm_nwr_signed_id_t> > set_delete_ids;
    for (const auto &id : deleted_node_ids)
      set_delete_ids.insert(std::make_pair(object_type::node, id));

    for (const auto &id : deleted_way_ids)
      set_delete_ids.insert(std::make_pair(object_type::way, id));

    for (const auto &id : deleted_relation_ids)
      set_delete_ids.insert(std::make_pair(object_type::relation, id));


    std::vector<diffresult_t> result;

    // Iterate over all elements in the sequence defined in the osmChange message

    for (auto &item : osmchange_orig_sequence) {

	switch (item.op) {

	  case operation::op_create:
	    {
	      auto it = map_create_ids.find(std::make_pair(item.obj_type, item.orig_id));
	      if (it != map_create_ids.end()) {
		  diffresult_t row{};
		  const auto id = it->second;
		  row.op = item.op;
		  row.obj_type = item.obj_type;
		  row.old_id = id.old_id;
		  row.new_id = id.new_id;
		  row.new_version = id.new_version;
		  row.deletion_skipped = false;
		  result.push_back(row);
	      }
	      else
		throw std::runtime_error ("Element in osmChange message was not processed");
	    }
	    break;

	  case operation::op_modify:
	    {
	      auto it = map_modify_ids.find(std::make_tuple(item.obj_type, item.orig_id, item.orig_version + 1));
	      if (it != map_modify_ids.end()) {
		  diffresult_t row{};
		  const auto id = it->second;
		  row.op = item.op;
		  row.obj_type = item.obj_type;
		  row.old_id = id.old_id;
		  row.new_id = id.new_id;
		  row.new_version = id.new_version;
		  row.deletion_skipped = false;
		  result.push_back(row);
	      }
	      else
		throw std::runtime_error ("Element in osmChange message was not processed");
	    }
	    break;

	  case operation::op_delete:

	    if (item.if_unused) {
		auto it = map_skip_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		if (it != map_skip_delete_ids.end()) {
		    diffresult_t row{};
		    const auto id = it->second;
		    row.op  = item.op;
		    row.obj_type = item.obj_type;
		    row.old_id = id.old_id;
		    row.new_id = id.new_id;
		    row.new_version = id.new_version;
		    row.deletion_skipped = true;
		    result.push_back(row);
		}
		else {
		    auto it = set_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		    if (it != set_delete_ids.end()) {
			diffresult_t row{};
			row.op = item.op;
			row.obj_type = item.obj_type;
			row.old_id = it->second;
			row.deletion_skipped = false;
			result.push_back(row);
		    }
		    else {
			throw std::runtime_error ("Element in osmChange message was not processed");
		    }
		}
	    }

	    if (!item.if_unused) {
		auto it = set_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		if (it != set_delete_ids.end()) {
		    diffresult_t row{};
		    row.op = item.op;
		    row.obj_type = item.obj_type;
		    row.old_id = it->second;
		    row.deletion_skipped = false;
		    result.push_back(row);
		}
		else {
		    throw std::runtime_error ("Element in osmChange message was not processed");
		}
	    }
	    break;

	  case operation::op_undefined:

	    throw std::runtime_error ("Unexpected operation in original sequence mapping. This should not happen!");

	}
    }

    return result;
  }

} // namespace api06
