/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/json_formatter.hpp"

#include <chrono>

json_formatter::json_formatter(std::unique_ptr<json_writer> w) : writer(std::move(w)) {}

json_formatter::~json_formatter() = default;

mime::type json_formatter::mime_type() const { return mime::type::application_json; }

void json_formatter::write_tags(const tags_t &tags) {

  if (tags.empty())
    return;

  writer->object_key("tags");
  writer->start_object();
  for (const auto& [key, value] : tags) {
    writer->property(key, value);
  }
  writer->end_object();
}


void json_formatter::start_document(
  const std::string &generator, const std::string &root_name) {
  writer->start_object();

  writer->property("version",     output_formatter::API_VERSION);
  writer->property("generator",   generator);
  writer->property("copyright",   output_formatter::COPYRIGHT);
  writer->property("attribution", output_formatter::ATTRIBUTION);
  writer->property("license",     output_formatter::LICENSE);
}

void json_formatter::write_bounds(const bbox &bounds) {
  writer->object_key("bounds");
  writer->start_object();
  writer->property("minlat", bounds.minlat);
  writer->property("minlon", bounds.minlon);
  writer->property("maxlat", bounds.maxlat);
  writer->property("maxlon", bounds.maxlon);
  writer->end_object();
}

void json_formatter::start_element() {

  writer->object_key("elements");
  writer->start_array();
}

//
void json_formatter::end_element() {

  writer->end_array();            // end of elements array
  writer->end_object();
}

void json_formatter::start_changeset(bool multi_selection) {

  if (multi_selection) {
    writer->object_key("changesets");
    writer->start_array();
  }
  else
  {
    writer->object_key("changeset");
  }
}

void json_formatter::end_changeset(bool multi_selection) {

  if (multi_selection) {
    writer->end_array();
  }
}


void json_formatter::end_document() {
  writer->end_object();
}

void json_formatter::start_action(action_type type) {
}

void json_formatter::end_action(action_type type) {
}

void json_formatter::error(const std::exception &e) {
  writer->start_object();
  writer->property("error", e.what());
  writer->end_object();
}

void json_formatter::write_id(const element_info &elem) {
  writer->property("id", elem.id);
}

void json_formatter::write_common(const element_info &elem) {
  writer->property("timestamp", elem.timestamp);
  writer->property("version",   elem.version);
  writer->property("changeset", elem.changeset);
  if (elem.display_name && elem.uid) {
    writer->property("user", *elem.display_name);
    writer->property("uid",  *elem.uid);
  }
  // At this itme, only the map call is really supported for JSON output,
  // where all elements are expected to be visible.
  if (!elem.visible) {
    writer->property("visible", elem.visible);
  }
}

void json_formatter::write_node(const element_info &elem, double lon,
                                double lat, const tags_t &tags) {
  writer->start_object();

  writer->property("type", "node");

  write_id(elem);
  if (elem.visible) {
    writer->property("lat", lat);
    writer->property("lon", lon);
  }
  write_common(elem);
  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                               const tags_t &tags) {
  writer->start_object();

  writer->property("type", "way");

  write_id(elem);
  write_common(elem);

  if (!nodes.empty()) {
      writer->object_key("nodes");
      writer->start_array();
      for (const auto &node : nodes) {
        writer->entry(node);
      }
      writer->end_array();
  }

  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_relation(const element_info &elem,
                                    const members_t &members,
                                    const tags_t &tags) {
  writer->start_object();

  writer->property("type", "relation");

  write_id(elem);
  write_common(elem);

  if (!members.empty()) {
      writer->object_key("members");
      writer->start_array();
      for (const auto & member : members) {
	  writer->start_object();
	  writer->property("type", element_type_name<std::string_view>(member.type));
	  writer->property("ref",  member.ref);
	  writer->property("role", member.role);
	  writer->end_object();
      }
      writer->end_array();
  }

  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_changeset(const changeset_info &cs,
                                     const tags_t &tags,
                                     bool include_comments,
                                     const comments_t &comments,
                                     const std::chrono::system_clock::time_point &now) {

  writer->start_object();
  writer->property("id", cs.id);
  writer->property("created_at", cs.created_at);

  const bool is_open = cs.is_open_at(now);

  writer->property("open", is_open);

  writer->property("comments_count", cs.comments_count);
  writer->property("changes_count", cs.num_changes);

  if (!is_open) {
      writer->property("closed_at", cs.closed_at);
  }

  if (cs.bounding_box) {
      writer->property("min_lat", cs.bounding_box->minlat);
      writer->property("min_lon", cs.bounding_box->minlon);
      writer->property("max_lat", cs.bounding_box->maxlat);
      writer->property("max_lon", cs.bounding_box->maxlon);
  }

  if (cs.display_name && bool(cs.uid)) {
    writer->property("uid", *cs.uid);
    writer->property("user", *cs.display_name);
  }

  write_tags(tags);

  if (include_comments && !comments.empty()) {
      writer->object_key("comments");
      writer->start_array();
      for (const auto & comment : comments) {
	  writer->start_object();
	  writer->property("id", comment.id);
	  writer->property("visible", true);
	  writer->property("date", comment.created_at);
	  writer->property("uid", comment.author_id);
	  writer->property("user", comment.author_display_name);
	  writer->property("text", comment.body);
	  writer->end_object();
      }
      writer->end_array();
  }
  writer->end_object();
}


void json_formatter::write_diffresult_create_modify(const element_type elem,
                                            const osm_nwr_signed_id_t old_id,
                                            const osm_nwr_id_t new_id,
                                            const osm_version_t new_version)
{

//  writer->start_object();
//  writer->object_key("type");
//  writer->entry_string(element_type_name(elem));
//  writer->object_key("old_id");
//  writer->entry_int(old_id);
//  writer->object_key("new_id");
//  writer->entry_int(new_id);
//  writer->object_key("new_version");
//  writer->entry_int(new_version);
//  writer->end_object();
}


void json_formatter::write_diffresult_delete(const element_type elem,
                                            const osm_nwr_signed_id_t old_id)
{
//  writer->start_object();
//  writer->object_key("type");
//  writer->entry_string(element_type_name(elem));
//  writer->object_key("old_id");
//  writer->entry_int(old_id);
//  writer->end_object();
}

void json_formatter::flush() { writer->flush(); }

void json_formatter::error(const std::string &s) { writer->error(s); }
