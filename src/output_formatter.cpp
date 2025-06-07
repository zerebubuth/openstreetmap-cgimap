/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/options.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/time.hpp"

#include <chrono>

element_info::element_info(osm_nwr_id_t id, osm_nwr_id_t version,
                           osm_changeset_id_t changeset,
                           const std::string &timestamp,
                           const std::optional<osm_user_id_t> &uid,
                           const std::optional<std::string> &display_name,
                           bool visible,
                           std::optional<osm_redaction_id_t> redaction)
  : id(id),
    version(version),
    changeset(changeset),
    timestamp(timestamp),
    uid(uid),
    display_name(display_name),
    visible(visible),
    redaction(redaction)
{}

changeset_info::changeset_info(osm_changeset_id_t id,
                               const std::string &created_at,
                               const std::string &closed_at,
                               const std::optional<osm_user_id_t> &uid,
                               const std::optional<std::string> &display_name,
                               const std::optional<bbox> &bounding_box,
                               size_t num_changes,
                               size_t comments_count)
  : id(id),
    created_at(created_at),
    closed_at(closed_at),
    uid(uid),
    display_name(display_name),
    bounding_box(bounding_box),
    num_changes(num_changes),
    comments_count(comments_count)
{}

bool changeset_info::is_open_at(const std::chrono::system_clock::time_point &now) const {
  const auto closed_at_time = parse_time(closed_at);
  return (closed_at_time > now) && (num_changes <= global_settings::get_changeset_max_elements());  // according to changeset.rb, is_open
}

changeset_comment_info::changeset_comment_info(osm_changeset_comment_id_t id,
                                               osm_user_id_t author_id,
                                               std::string body,
                                               std::string created_at,
                                               std::string author_display_name)
  : id(id),
    author_id(author_id),
    body(std::move(body)),
    created_at(std::move(created_at)),
    author_display_name(std::move(author_display_name))
{}

member_info::member_info(element_type type, osm_nwr_id_t ref, std::string role)
  : type(type),
    ref(ref),
    role(std::move(role))
{}
