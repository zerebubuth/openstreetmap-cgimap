/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/api06/changeset_upload/osmchange_handler.hpp"

#include "cgimap/http.hpp"

#include <fmt/core.h>

namespace api06 {

OSMChange_Handler::OSMChange_Handler(Node_Updater& node_updater,
                                     Way_Updater& way_updater,
                                     Relation_Updater& relation_updater,
                                     osm_changeset_id_t changeset)
    : node_updater(node_updater),
      way_updater(way_updater),
      relation_updater(relation_updater),
      changeset(changeset)
{}

void OSMChange_Handler::start_document() {}

void OSMChange_Handler::end_document() { finish_processing(); }

// checks common to all objects
void OSMChange_Handler::check_osm_object(const OSMObject &o) const {

  if (o.changeset() != changeset)
    throw http::conflict(
        fmt::format(
             "Changeset mismatch: Provided {:d} but only {:d} is allowed",
         o.changeset(), changeset));
}

void OSMChange_Handler::process_node(const Node &node, 
                                     operation op,
                                     bool if_unused) {

  assert(op != operation::op_undefined);

  check_osm_object(node);

  switch (op) {
  case operation::op_create:
    handle_new_state(state::st_create_node);
    node_updater.add_node(node.lat(), node.lon(), changeset, node.id(),
                           node.tags());
    break;

  case operation::op_modify:
    handle_new_state(state::st_modify);
    node_updater.modify_node(node.lat(), node.lon(), changeset, node.id(),
                              node.version(), node.tags());
    break;

  case operation::op_delete:
    handle_new_state(state::st_delete_node);
    node_updater.delete_node(changeset, node.id(), node.version(),
                              if_unused);
    break;

  default:
    // handled by assertion
    break;
  }
}

void OSMChange_Handler::process_way(const Way &way, operation op,
                                    bool if_unused) {

  assert(op != operation::op_undefined);

  check_osm_object(way);

  switch (op) {
  case operation::op_create:
    handle_new_state(state::st_create_way);
    way_updater.add_way(changeset, way.id(), way.nodes(), way.tags());
    break;

  case operation::op_modify:
    handle_new_state(state::st_modify);
    way_updater.modify_way(changeset, way.id(), way.version(), way.nodes(),
                            way.tags());
    break;

  case operation::op_delete:
    handle_new_state(state::st_delete_way);
    way_updater.delete_way(changeset, way.id(), way.version(), if_unused);
    break;

  default:
    // handled by assertion
    break;
  }
}

void OSMChange_Handler::process_relation(const Relation &relation, 
                                         operation op,
                                         bool if_unused) {

  assert(op != operation::op_undefined);

  check_osm_object(relation);

  switch (op) {
  case operation::op_create:
    handle_new_state(state::st_create_relation);
    relation_updater.add_relation(changeset, relation.id(),
                                   relation.members(), relation.tags());
    break;

  case operation::op_modify:
    handle_new_state(state::st_modify);
    relation_updater.modify_relation(changeset, relation.id(),
                                      relation.version(), relation.members(),
                                      relation.tags());
    break;

  case operation::op_delete:
    handle_new_state(state::st_delete_relation);
    relation_updater.delete_relation(changeset, relation.id(),
                                      relation.version(), if_unused);
    break;
  default:
    // handled by assertion
    break;
  }
}

void OSMChange_Handler::finish_processing() {
  handle_new_state(state::st_finished);
}

uint32_t OSMChange_Handler::get_num_changes() const {
  return (node_updater.get_num_changes() + 
          way_updater.get_num_changes() +
          relation_updater.get_num_changes());
}

bbox_t OSMChange_Handler::get_bbox() const {
  bbox_t bbox;

  bbox.expand(node_updater.bbox());
  bbox.expand(way_updater.bbox());
  bbox.expand(relation_updater.bbox());

  return bbox;
}

void OSMChange_Handler::handle_new_state(state new_state) {

  // TODO: add flush_limit to send data to db if too much memory is used for
  // internal buffers
  if (new_state == current_state /* && object_counter < flush_limit */)
    return;

  // process objects in buffer for the current state before doing a transition
  // to the new state
  switch (current_state) {
  case state::st_initial:
    // nothing to do
    break;

  case state::st_create_node:
    node_updater.process_new_nodes();
    break;

  case state::st_create_way:
    way_updater.process_new_ways();
    break;

  case state::st_create_relation:
    relation_updater.process_new_relations();
    break;

  case state::st_modify:
    node_updater.process_modify_nodes();
    way_updater.process_modify_ways();
    relation_updater.process_modify_relations();
    break;

  case state::st_delete_node:
    node_updater.process_delete_nodes();
    break;

  case state::st_delete_way:
    way_updater.process_delete_ways();
    break;

  case state::st_delete_relation:
    relation_updater.process_delete_relations();
    break;

  case state::st_finished:
    // nothing to do
    break;
  }

  // complete transition to new state
  current_state = new_state;
}

} // namespace api06
