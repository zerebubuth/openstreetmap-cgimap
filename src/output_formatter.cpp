/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/options.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/time.hpp"

#include <chrono>

element_info::element_info(osm_nwr_id_t id_, osm_nwr_id_t version_,
                           osm_changeset_id_t changeset_,
                           const std::string &timestamp_,
                           const std::optional<osm_user_id_t> &uid_,
                           const std::optional<std::string> &display_name_,
                           bool visible_,
                           std::optional<osm_redaction_id_t> redaction_)
  : id(id_), 
    version(version_), 
    changeset(changeset_),
    timestamp(timestamp_), 
    uid(uid_), 
    display_name(display_name_),
    visible(visible_), 
    redaction(redaction_) 
{}

changeset_info::changeset_info(osm_changeset_id_t id_,
                               const std::string &created_at_,
                               const std::string &closed_at_,
                               const std::optional<osm_user_id_t> &uid_,
                               const std::optional<std::string> &display_name_,
                               const std::optional<bbox> &bounding_box_,
                               size_t num_changes_,
                               size_t comments_count_)
  : id(id_),
    created_at(created_at_),
    closed_at(closed_at_),
    uid(uid_),
    display_name(display_name_),
    bounding_box(bounding_box_),
    num_changes(num_changes_),
    comments_count(comments_count_)
{}

bool changeset_info::is_open_at(const std::chrono::system_clock::time_point &now) const {
  const std::chrono::system_clock::time_point closed_at_time = parse_time(closed_at);
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

member_info::member_info(element_type type_, osm_nwr_id_t ref_, std::string role_)
  : type(type_),
    ref(ref_),
    role(std::move(role_))
{}
