/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OUTPUT_FORMATTER_HPP
#define OUTPUT_FORMATTER_HPP

#include "cgimap/bbox.hpp"
#include "cgimap/types.hpp"
#include "cgimap/mime_types.hpp"

#include <chrono>
#include <optional>
#include <vector>

/**
 * What type of element the formatter is starting to write.
 */
enum class element_type {
  changeset,
  node,
  way,
  relation
};

enum class action_type {
  create,
  modify,
  del    // delete is a reserved keyword
};

namespace {

template <typename T = const char *>
constexpr T element_type_name(element_type elt) noexcept {

  switch (elt) {
  case element_type::node:
    return "node";
  case element_type::way:
    return "way";
  case element_type::relation:
    return "relation";
  case element_type::changeset:
    return "changeset";
  }
  return "";
}

} // anonymous namespace

struct element_info {

  element_info() = default;

  element_info(osm_nwr_id_t id_, osm_nwr_id_t version_,
               osm_changeset_id_t changeset_,
               const std::string &timestamp_,
               const std::optional<osm_user_id_t> &uid_,
               const std::optional<std::string> &display_name_,
               bool visible_,
               std::optional<osm_redaction_id_t> redaction_ = {});

  bool operator==(const element_info &other) const = default;

  // Standard meanings
  osm_nwr_id_t id = 0;
  osm_nwr_id_t version = 0;
  osm_changeset_id_t changeset = 0;
  std::string timestamp;
  // Anonymous objects will not have uids or display names
  std::optional<osm_user_id_t> uid;
  std::optional<std::string> display_name{};
  // If an object has been deleted
  bool visible = false;
  // If an object has administratively hidden in a "redaction". note that this
  // is never output - if it is present, then the element should not be
  // displayed except to moderators.
  std::optional<osm_redaction_id_t> redaction = {};
};

struct changeset_info {
  changeset_info() = default;
  changeset_info(const changeset_info &) = default;
  changeset_info(osm_changeset_id_t id_,
                 const std::string &created_at_,
                 const std::string &closed_at_,
                 const std::optional<osm_user_id_t> &uid_,
                 const std::optional<std::string> &display_name_,
                 const std::optional<bbox> &bounding_box_,
                 size_t num_changes_,
                 size_t comments_count_);

  bool operator==(const changeset_info &other) const = default;

  // returns true if the changeset is "open" at a particular
  // point in time.
  //
  // note that the definition of "open" is fraught with
  // difficulty, and it's not wise to rely on it too much.
  bool is_open_at(const std::chrono::system_clock::time_point &) const;

  // standard meaning of ID
  osm_changeset_id_t id{0};
  // changesets are created at a certain time and may be either
  // closed explicitly with a closing time, or close implicitly
  // an hour after the last update to the changeset. closed_at
  // should have an ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
  std::string created_at;
  std::string closed_at;
  // anonymous objects don't have UIDs or display names
  std::optional<osm_user_id_t> uid;
  std::optional<std::string> display_name;
  // changesets with edits will have a bounding box containing
  // the extent of all the changes.
  std::optional<bbox> bounding_box;
  // the number of changes (new element versions) associated
  // with this changeset.
  size_t num_changes{0};
  // if the changeset has a discussion attached, then this will
  // be the number of comments.
  size_t comments_count{0};
};

struct changeset_comment_info {
  osm_changeset_comment_id_t id;
  osm_user_id_t author_id;
  std::string body;
  std::string created_at;
  std::string author_display_name;

  changeset_comment_info() = default;
  changeset_comment_info(const changeset_comment_info&) = default;
  changeset_comment_info(osm_changeset_comment_id_t id,
                         osm_user_id_t author_id,
                         std::string body,
                         std::string created_at,
                         std::string author_display_name);

  bool operator==(const changeset_comment_info &other) const = default;
};

struct member_info {
  element_type type;
  osm_nwr_id_t ref;
  std::string role;

  member_info() = default;
  member_info(element_type type_, osm_nwr_id_t ref_, std::string role_);

  bool operator==(const member_info &other) const = default;
};

using nodes_t = std::vector<osm_nwr_id_t>;
using members_t = std::vector<member_info>;
using tags_t = std::vector<std::pair<std::string, std::string> >;
using comments_t = std::vector<changeset_comment_info>;

/**
 * Base type for different output formats. Hopefully this is general
 * enough to encompass most formats that we want to produce. Assuming,
 * of course, that we want any other formats ;-)
 */
struct output_formatter {
  static constexpr const char * API_VERSION = "0.6";
  static constexpr const char * COPYRIGHT = "OpenStreetMap and contributors";
  static constexpr const char * ATTRIBUTION = "http://www.openstreetmap.org/copyright";
  static constexpr const char * LICENSE = "http://opendatacommons.org/licenses/odbl/1-0/";

  virtual ~output_formatter() = default;

  // returns the mime type of the content that this formatter will
  // produce.
  virtual mime::type mime_type() const = 0;

  // called once to start the document - this will be the first call to this
  // object after construction. the first argument will be used as the
  // "generator" header attribute, and the second will name the root element
  // (if there is one - JSON doesn't have one), e.g: "osm" or "osmChange".
  virtual void start_document(
    const std::string &generator, const std::string &root_name) = 0;

  // called once to end the document - there will be no calls after this
  // one. this will be called, even if an error has occurred.
  virtual void end_document() = 0;

  // this is called if there is an error during reading data from the
  // database. hopefully this is a very very rare occurrance.
  virtual void error(const std::exception &e) = 0;

  // write a bounds object to the document. this seems to be generally used
  // to record the box used by a map call.
  virtual void write_bounds(const bbox &bounds) = 0;

  //
  virtual void start_element() = 0;

  //
  virtual void end_element() = 0;

  virtual void start_changeset(bool) = 0;

  virtual void end_changeset(bool) = 0;

  // TODO: document me.
  virtual void start_action(action_type type) = 0;
  virtual void end_action(action_type type) = 0;

  // output a single node given that node's row and an iterator over its tags
  virtual void write_node(const element_info &elem, double lon, double lat,
                          const tags_t &tags) = 0;

  // output a single way given a row and iterators for nodes and tags
  virtual void write_way(const element_info &elem, const nodes_t &nodes,
                         const tags_t &tags) = 0;

  // output a single relation given a row and iterators over members and tags
  virtual void write_relation(const element_info &elem,
                              const members_t &members, const tags_t &tags) = 0;

  // output a single changeset.
  virtual void write_changeset(const changeset_info &elem,
                               const tags_t &tags,
                               bool include_comments,
                               const comments_t &comments,
                               const std::chrono::system_clock::time_point &now) = 0;

  // output an entry of a diffResult with 3 parameters: old_id, new_id and new_version
  virtual void write_diffresult_create_modify(const element_type elem,
                                              const osm_nwr_signed_id_t old_id,
                                              const osm_nwr_id_t new_id,
                                              const osm_version_t new_version) = 0;

  // output an entry of a diffResult with 1 parameter: old_id
  virtual void write_diffresult_delete(const element_type elem,
                                       const osm_nwr_signed_id_t old_id) = 0;

  // flush the current state
  virtual void flush() = 0;

  // write an error to the output stream
  virtual void error(const std::string &) = 0;
};

#endif /* OUTPUT_FORMATTER_HPP */
