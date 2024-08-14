/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSMCHANGE_HANDLER_HPP
#define OSMCHANGE_HANDLER_HPP

#include "cgimap/api06/changeset_upload/node_updater.hpp"
#include "cgimap/api06/changeset_upload/relation_updater.hpp"
#include "cgimap/api06/changeset_upload/way_updater.hpp"

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include "node.hpp"
#include "osmobject.hpp"
#include "parser_callback.hpp"
#include "relation.hpp"
#include "way.hpp"

namespace api06 {

class OSMChange_Handler : public Parser_Callback {

public:
  OSMChange_Handler(Node_Updater&,
                    Way_Updater&,
                    Relation_Updater&,
                    osm_changeset_id_t changeset);

  ~OSMChange_Handler() override = default;

  // checks common to all objects
  void check_osm_object(const OSMObject &o) const;

  void start_document() override;

  void end_document() override;

  void process_node(const Node &node, operation op, bool if_unused) override;

  void process_way(const Way &way, operation op, bool if_unused) override;

  void process_relation(const Relation &relation, operation op, bool if_unused) override;

  unsigned int get_num_changes() const;

  bbox_t get_bbox() const;

private:
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

  void handle_new_state(state new_state);

  void finish_processing();

  state current_state{ state::st_initial };

  Node_Updater& node_updater;
  Way_Updater& way_updater;
  Relation_Updater& relation_updater;

  osm_changeset_id_t changeset;
};

} // namespace api06

#endif
