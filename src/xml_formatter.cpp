/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/xml_formatter.hpp"

#include <string>
#include <utility>


xml_formatter::xml_formatter(std::unique_ptr<xml_writer> w) : writer(std::move(w)) {}

mime::type xml_formatter::mime_type() const { return mime::type::application_xml; }

void xml_formatter::start_document(
  const std::string &generator, const std::string &root_name) {
  writer->start(root_name);
  writer->attribute("version", output_formatter::API_VERSION);
  writer->attribute("generator", generator);

  writer->attribute("copyright", output_formatter::COPYRIGHT);
  writer->attribute("attribution", output_formatter::ATTRIBUTION);
  writer->attribute("license", output_formatter::LICENSE);
}

void xml_formatter::end_document() { writer->end(); }

void xml_formatter::write_bounds(const bbox &bounds) {
  // bounds element, which seems to be standard in the
  // main map call.
  writer->start("bounds");
  writer->attribute("minlat", bounds.minlat);
  writer->attribute("minlon", bounds.minlon);
  writer->attribute("maxlat", bounds.maxlat);
  writer->attribute("maxlon", bounds.maxlon);
  writer->end();
}

//
void xml_formatter::start_element() {
  // nothing to do for xml
}

void xml_formatter::end_element() {
  // nothing to do for xml
}

void xml_formatter::start_changeset(bool) {
  // nothing to do for xml
}

void xml_formatter::end_changeset(bool) {
  // nothing to do for xml
}

void xml_formatter::start_action(action_type type) {
  switch (type) {
  case action_type::create:
    writer->start("create");
    break;
  case action_type::modify:
    writer->start("modify");
    break;
  case action_type::del:
    writer->start("delete");
    break;
  }
}

void xml_formatter::end_action(action_type type) {
  switch (type) {
  case action_type::create:
  case action_type::modify:
  case action_type::del:
    writer->end();
    break;
  }
}

void xml_formatter::error(const std::exception &e) {
  // write out an error element to the xml file - we've probably
  // already started writing, so its useless to try and alter the
  // HTTP code.
  writer->start("error");
  writer->text(e.what());
  writer->end();
}

void xml_formatter::write_tags(const tags_t &tags) {
  for (const auto & [key, value] : tags) {
    writer->start("tag");
    writer->attribute("k", key);
    writer->attribute("v", value);
    writer->end();
  }
}

void xml_formatter::write_common(const element_info &elem) {
  writer->attribute("id", elem.id);
  writer->attribute("visible", elem.visible);
  writer->attribute("version", elem.version);
  writer->attribute("changeset", elem.changeset);
  writer->attribute("timestamp", elem.timestamp);
  if (elem.display_name && elem.uid) {
    writer->attribute("user", *elem.display_name);
    writer->attribute("uid", *elem.uid);
  }
}

void xml_formatter::write_node(const element_info &elem, double lon, double lat,
                               const tags_t &tags) {
  writer->start("node");
  write_common(elem);
  if (elem.visible) {
    writer->attribute("lat", lat);
    writer->attribute("lon", lon);
  }
  write_tags(tags);

  writer->end();
}

void xml_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                              const tags_t &tags) {
  writer->start("way");
  write_common(elem);

  for (osm_nwr_id_t node : nodes) {
    writer->start("nd");
    writer->attribute("ref", node);
    writer->end();
  }

  write_tags(tags);

  writer->end();
}

void xml_formatter::write_relation(const element_info &elem,
                                   const members_t &members,
                                   const tags_t &tags) {
  writer->start("relation");
  write_common(elem);

  for (const auto & member : members) {
    writer->start("member");
    writer->attribute("type", element_type_name(member.type));
    writer->attribute("ref", member.ref);
    writer->attribute("role", member.role);
    writer->end();
  }

  write_tags(tags);

  writer->end();
}

void xml_formatter::write_changeset(const changeset_info &elem,
                                    const tags_t &tags,
                                    bool include_comments,
                                    const comments_t &comments,
                                    const std::chrono::system_clock::time_point &now) {
  writer->start("changeset");

  writer->attribute("id", elem.id);
  writer->attribute("created_at", elem.created_at);
  const bool is_open = elem.is_open_at(now);
  if (!is_open) {
    writer->attribute("closed_at", elem.closed_at);
  }
  writer->attribute("open", is_open);

  if (bool(elem.display_name) && bool(elem.uid)) {
    writer->attribute("user", *elem.display_name);
    writer->attribute("uid", *elem.uid);
  }

  if (elem.bounding_box) {
    writer->attribute("min_lat", elem.bounding_box->minlat);
    writer->attribute("min_lon", elem.bounding_box->minlon);
    writer->attribute("max_lat", elem.bounding_box->maxlat);
    writer->attribute("max_lon", elem.bounding_box->maxlon);
  }

  writer->attribute("comments_count", elem.comments_count);
  writer->attribute("changes_count", elem.num_changes);

  write_tags(tags);

  if (include_comments) {
    writer->start("discussion");
    for (const auto & comment : comments) {
      writer->start("comment");
      writer->attribute("id", comment.id);
      writer->attribute("date", comment.created_at);
      writer->attribute("uid", comment.author_id);
      writer->attribute("user", comment.author_display_name);
      writer->start("text");
      writer->text(comment.body);
      writer->end();
      writer->end();
    }
    writer->end();
  }

  writer->end();
}

void xml_formatter::write_diffresult_create_modify(const element_type elem,
                                            const osm_nwr_signed_id_t old_id,
                                            const osm_nwr_id_t new_id,
                                            const osm_version_t new_version)
{
  writer->start(element_type_name(elem));
  writer->attribute("old_id", old_id);
  writer->attribute("new_id", new_id);
  writer->attribute("new_version", new_version);
  writer->end();
}


void xml_formatter::write_diffresult_delete(const element_type elem,
                                            const osm_nwr_signed_id_t old_id)
{
  writer->start(element_type_name(elem));
  writer->attribute("old_id", old_id);
  writer->end();
}



void xml_formatter::flush() { writer->flush(); }

void xml_formatter::error(const std::string &s) { writer->error(s); }
