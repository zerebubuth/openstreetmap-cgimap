/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSMCHANGE_TRACKING_HPP
#define OSMCHANGE_TRACKING_HPP

#include <vector>

#include "cgimap/types.hpp"

namespace api06 {


struct diffresult_t {
  operation op;
  object_type obj_type;
  osm_nwr_signed_id_t old_id;
  osm_nwr_id_t new_id;
  osm_version_t new_version;
  bool deletion_skipped;
};

class OSMChange_Tracking {

public:

  OSMChange_Tracking() = default;

  std::vector<diffresult_t> assemble_diffresult();

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
  };

  // created objects are kept separately for id replacement purposes
  std::vector<object_id_mapping_t> created_node_ids;
  std::vector<object_id_mapping_t> created_way_ids;
  std::vector<object_id_mapping_t> created_relation_ids;

  std::vector<object_id_mapping_t> modified_node_ids;
  std::vector<object_id_mapping_t> modified_way_ids;
  std::vector<object_id_mapping_t> modified_relation_ids;

  std::vector<osm_nwr_signed_id_t> deleted_node_ids;
  std::vector<osm_nwr_signed_id_t> deleted_way_ids;
  std::vector<osm_nwr_signed_id_t> deleted_relation_ids;

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

} // namespace api06

#endif
