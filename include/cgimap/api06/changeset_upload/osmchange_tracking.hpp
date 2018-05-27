#ifndef OSMCHANGE_TRACKING_HPP
#define OSMCHANGE_TRACKING_HPP

#include <map>
#include <string>
#include <vector>

#include "types.hpp"

class OSMChange_Tracking {

public:
  struct object_id_mapping_t {
    osm_nwr_signed_id_t old_id;
    osm_nwr_id_t new_id;
    osm_version_t new_version;
  };

  struct osmchange_t {
    operation op;
    object_type obj_type;
    osm_nwr_signed_id_t orig_id;
    osm_version_t orig_version;
    bool if_unused;
    // the following fields will be populated once the whole message has been processed
    object_id_mapping_t mapping;
    bool deletion_skipped;         // if-unused flag was set, and object could not be deleted

  };

  OSMChange_Tracking() {};

  void populate_orig_sequence_mapping() {

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
    std::set< std::pair< object_type, osm_nwr_id_t> > set_delete_ids;
    for (const auto &id : deleted_node_ids)
      set_delete_ids.insert(std::make_pair(object_type::node, id));

    for (const auto &id : deleted_way_ids)
      set_delete_ids.insert(std::make_pair(object_type::way, id));

    for (const auto &id : deleted_relation_ids)
      set_delete_ids.insert(std::make_pair(object_type::relation, id));


    // Iterate over all elements in the sequence defined in the osmChange message

    for (auto &item : osmchange_orig_sequence) {

	switch (item.op) {

	  case operation::op_create:
	    {
	      auto it = map_create_ids.find(std::make_pair(item.obj_type, item.orig_id));
	      if (it != map_create_ids.end()) {
		  const auto id = it->second;
		  item.mapping.old_id = id.old_id;
		  item.mapping.new_id = id.new_id;
		  item.mapping.new_version = id.new_version;
	      }
	      else
		throw std::runtime_error ("Element in osmChange message was not processed");
	    }
	    break;

	  case operation::op_modify:
	    {
	      auto it = map_modify_ids.find(std::make_tuple(item.obj_type, item.orig_id, item.orig_version + 1));
	      if (it != map_modify_ids.end()) {
		  const auto id = it->second;
		  item.mapping.old_id = id.old_id;
		  item.mapping.new_id = id.new_id;
		  item.mapping.new_version = id.new_version;
	      }
	      else
		throw std::runtime_error ("Element in osmChange message was not processed");
	    }
	    break;

	  case operation::op_delete:

	    if (item.if_unused) {
		auto it = map_skip_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		if (it != map_skip_delete_ids.end()) {

		    const auto id = it->second;
		    item.mapping.old_id = id.old_id;
		    item.mapping.new_id = id.new_id;
		    item.mapping.new_version = id.new_version;
		    item.deletion_skipped = true;
		}
		else {

		    auto it = set_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		    if (it != set_delete_ids.end()) {
			item.deletion_skipped = false;
		    }
		    else {
			throw std::runtime_error ("Element in osmChange message was not processed");
		    }
		}
	    }

	    if (!item.if_unused) {

		auto it = set_delete_ids.find(std::make_pair(item.obj_type, item.orig_id));
		if (it != set_delete_ids.end()) {

		    item.deletion_skipped = false;
		}
		else {
		    throw std::runtime_error ("Element in osmChange message was not processed");
		}
	    }
	    break;

	}
    }
  }

  // created objects are kept separately for id replacement purposes
  std::vector<object_id_mapping_t> created_node_ids;
  std::vector<object_id_mapping_t> created_way_ids;
  std::vector<object_id_mapping_t> created_relation_ids;

  std::vector<object_id_mapping_t> modified_node_ids;
  std::vector<object_id_mapping_t> modified_way_ids;
  std::vector<object_id_mapping_t> modified_relation_ids;

  std::vector<osm_nwr_id_t> deleted_node_ids;
  std::vector<osm_nwr_id_t> deleted_way_ids;
  std::vector<osm_nwr_id_t> deleted_relation_ids;

  // in case the caller has provided an "if-unused" flag and requests
  // deletion for objects which are either (a) already deleted or
  // (b) still in used by another object, we have to return
  // old_id, new_id and version instead of raising an error message
  std::vector<object_id_mapping_t> skip_deleted_node_ids;
  std::vector<object_id_mapping_t> skip_deleted_way_ids;
  std::vector<object_id_mapping_t> skip_deleted_relation_ids;

  // Some clients might expect diffResult to reflect the original
  // object sequence as provided in the osmChange message
  // the following vector keeps a copy of that original sequence
  std::vector<osmchange_t> osmchange_orig_sequence;
};

#endif
