/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef API06_CHANGESET_UPLOAD_RELATION_UPDATER
#define API06_CHANGESET_UPLOAD_RELATION_UPDATER

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "cgimap/api06/changeset_upload/relation.hpp"

#include <map>
#include <string>
#include <vector>

namespace api06 {

using RelationMemberList = std::vector<RelationMember>;
using TagList = std::map<std::string, std::string>;

class Relation_Updater {

public:
  virtual ~Relation_Updater() = default;

  virtual void add_relation(osm_changeset_id_t changeset_id,
                            osm_nwr_signed_id_t old_id,
                            const RelationMemberList &members,
                            const TagList &tags) = 0;

  virtual void modify_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                               osm_version_t version,
                               const RelationMemberList &members,
                               const TagList &tags) = 0;

  virtual void delete_relation(osm_changeset_id_t changeset_id, osm_nwr_id_t id,
                               osm_version_t version, bool if_unused) = 0;

  virtual void process_new_relations() = 0;

  virtual void process_modify_relations() = 0;

  virtual void process_delete_relations() = 0;

  virtual uint32_t get_num_changes() const = 0;

  virtual bbox_t bbox() const = 0;
};

} // namespace api06

#endif /* API06_CHANGESET_UPLOAD_RELATION_UPDATER */
