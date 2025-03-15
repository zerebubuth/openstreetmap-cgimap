/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/common_pgsql_selection.hpp"
#include "cgimap/backend/apidb/utils.hpp"
#include "cgimap/options.hpp"

#include <chrono>

namespace {

using pqxx_tuple = pqxx::result::reference;
using pqxx_field = pqxx::field;

struct elem_columns
{
  elem_columns(const pqxx::result &rows) :

    id_col(rows.column_number("id")),
    version_col(rows.column_number("version")),
    timestamp_col(rows.column_number("timestamp")),
    changeset_id_col(rows.column_number("changeset_id")),
    visible_col(rows.column_number("visible")) {}

  const pqxx::row_size_type id_col;
  const pqxx::row_size_type version_col;
  const pqxx::row_size_type timestamp_col;
  const pqxx::row_size_type changeset_id_col;
  const pqxx::row_size_type visible_col;
};

struct changeset_columns
{
  changeset_columns(const pqxx::result &rows) :
    id_col(rows.column_number("id")),
    created_at_col(rows.column_number("created_at")),
    closed_at_col(rows.column_number("closed_at")),
    min_lat_col(rows.column_number("min_lat")),
    max_lat_col(rows.column_number("max_lat")),
    min_lon_col(rows.column_number("min_lon")),
    max_lon_col(rows.column_number("max_lon")),
    num_changes_col(rows.column_number("num_changes"))
  {}

   const pqxx::row_size_type id_col;
   const pqxx::row_size_type created_at_col;
   const pqxx::row_size_type closed_at_col;
   const pqxx::row_size_type min_lat_col;
   const pqxx::row_size_type max_lat_col;
   const pqxx::row_size_type min_lon_col;
   const pqxx::row_size_type max_lon_col;
   const pqxx::row_size_type num_changes_col;
};

struct tag_columns
{
  tag_columns(const pqxx::result &rows) :
    tag_k_col(rows.column_number("tag_k")),
    tag_v_col(rows.column_number("tag_v"))
  {}

  const pqxx::row_size_type tag_k_col;
  const pqxx::row_size_type tag_v_col;
};

struct node_extra_columns
{
  node_extra_columns(const pqxx::result &rows) :
    longitude_col(rows.column_number("longitude")),
    latitude_col(rows.column_number("latitude"))
 {}

  const pqxx::row_size_type longitude_col;
  const pqxx::row_size_type latitude_col;
};

struct way_extra_columns
{
  way_extra_columns(const pqxx::result &rows) :
    node_ids_col(rows.column_number("node_ids"))
 {}

  const pqxx::row_size_type node_ids_col;
};

struct relation_extra_columns
{
  relation_extra_columns(const pqxx::result &rows) :
    member_types_col(rows.column_number("member_types")),
    member_ids_col(rows.column_number("member_ids")),
    member_roles_col(rows.column_number("member_roles"))
 {}

  const pqxx::row_size_type member_types_col;
  const pqxx::row_size_type member_ids_col;
  const pqxx::row_size_type member_roles_col;
};

struct comments_columns
{
  comments_columns(const pqxx::result &rows) :

    comment_id_col(rows.column_number("comment_id")),
    comment_author_id_col(rows.column_number("comment_author_id")),
    comment_display_name_col(rows.column_number("comment_display_name")),
    comment_body_col(rows.column_number("comment_body")),
    comment_created_at_col(rows.column_number("comment_created_at")) {}

  const pqxx::row_size_type comment_id_col;
  const pqxx::row_size_type comment_author_id_col;
  const pqxx::row_size_type comment_display_name_col;
  const pqxx::row_size_type comment_body_col;
  const pqxx::row_size_type comment_created_at_col;
};

// -------------------------------------------------------------------------------------

[[nodiscard]] element_info extract_elem(const pqxx_tuple &row,
                  std::map<osm_changeset_id_t, changeset> &changeset_cache,
                  const elem_columns& col) {

  element_info elem;

  elem.id        = row[col.id_col].as<osm_nwr_id_t>();
  elem.version   = row[col.version_col].as<int>();
  elem.timestamp = row[col.timestamp_col].as<std::string>();
  elem.changeset = row[col.changeset_id_col].as<osm_changeset_id_t>();
  elem.visible   = row[col.visible_col].as<bool>();

  changeset & cs = changeset_cache[elem.changeset];

  if (cs.data_public) {
    elem.uid = cs.user_id;
    elem.display_name = cs.display_name;
  } else {
    elem.uid = {};
    elem.display_name = {};
  }
  return elem;
}

template <typename T>
std::optional<T> extract_optional(const pqxx_field &f) {
  if (f.is_null()) {
    return {};
  } else {
    return f.as<T>();
  }
}

[[nodiscard]] changeset_info extract_changeset(const pqxx_tuple &row,
                       std::map<osm_changeset_id_t, changeset> &changeset_cache,
                       const changeset_columns& col) {

  changeset_info elem;

  elem.id = row[col.id_col].as<osm_changeset_id_t>();
  elem.created_at = row[col.created_at_col].as<std::string>();
  elem.closed_at = row[col.closed_at_col].as<std::string>();

  const auto & cs = changeset_cache[elem.id];

  if (cs.data_public) {
    elem.uid = cs.user_id;
    elem.display_name = cs.display_name;
  } else {
    elem.uid = {};
    elem.display_name = {};
  }

  auto min_lat = extract_optional<int64_t>(row[col.min_lat_col]);
  auto max_lat = extract_optional<int64_t>(row[col.max_lat_col]);
  auto min_lon = extract_optional<int64_t>(row[col.min_lon_col]);
  auto max_lon = extract_optional<int64_t>(row[col.max_lon_col]);

  if (bool(min_lat) && bool(min_lon) && bool(max_lat) && bool(max_lon)) {
    elem.bounding_box = bbox(double(*min_lat) / global_settings::get_scale(),
                             double(*min_lon) / global_settings::get_scale(),
                             double(*max_lat) / global_settings::get_scale(),
                             double(*max_lon) / global_settings::get_scale());
  } else {
    elem.bounding_box = {};
  }

  elem.num_changes = row[col.num_changes_col].as<size_t>();

  return elem;
}

[[nodiscard]] tags_t extract_tags(const pqxx_tuple &row, const tag_columns& col) {

  tags_t tags;

  auto keys   = psql_array_to_vector(row[col.tag_k_col]);
  auto values = psql_array_to_vector(row[col.tag_v_col]);

  if (keys.size() != values.size()) {
    throw std::runtime_error("Mismatch in tags key and value size");
  }

  tags.reserve(keys.size());

  for (std::size_t i = 0; i < keys.size(); i++)
    tags.emplace_back(std::move(keys[i]), std::move(values[i]));

  return tags;
}

[[nodiscard]] nodes_t extract_way_nodes(const pqxx_tuple &row, const way_extra_columns& col) {

  return psql_array_ids_to_vector<osm_nwr_id_t>(row[col.node_ids_col]);
}

element_type type_from_name(const char *name) {
  element_type type{};

  switch (name[0]) {
  case 'N':
  case 'n':
    type = element_type::node;
    break;

  case 'W':
  case 'w':
    type = element_type::way;
    break;

  case 'R':
  case 'r':
    type = element_type::relation;
    break;

  default:
    // in case the name match isn't exhaustive...
    throw std::runtime_error(
        "Unexpected name not matched to type in type_from_name().");
  }

  return type;
}

[[nodiscard]] members_t extract_members(const pqxx_tuple &row, const relation_extra_columns& col) {

  members_t members;

  auto ids   = psql_array_ids_to_vector<osm_nwr_id_t>(row[col.member_ids_col]);
  auto types = psql_array_to_vector(row[col.member_types_col], ids.size());
  auto roles = psql_array_to_vector(row[col.member_roles_col], ids.size());

  if (types.size() != ids.size() ||
      ids.size() != roles.size()) {
    throw std::runtime_error("Mismatch in members types, ids and roles size");
  }

  members.reserve(ids.size());

  for (std::size_t i=0; i<ids.size(); i++) {
    element_type member_type = type_from_name(types[i].c_str());
    members.emplace_back(member_type, ids[i], std::move(roles[i]));
  }

  return members;
}

[[nodiscard]] comments_t extract_comments(const pqxx_tuple &row, const comments_columns& col) {

  comments_t comments;

  auto id           = psql_array_ids_to_vector<osm_changeset_comment_id_t>(row[col.comment_id_col]);
  auto author_id    = psql_array_ids_to_vector<osm_user_id_t>(row[col.comment_author_id_col]);
  auto display_name = psql_array_to_vector(row[col.comment_display_name_col], id.size());
  auto body         = psql_array_to_vector(row[col.comment_body_col], id.size());
  auto created_at   = psql_array_to_vector(row[col.comment_created_at_col], id.size());

  if (id.size() != author_id.size() ||
      author_id.size() != display_name.size() ||
      display_name.size() != body.size() ||
      body.size() != created_at.size()) {
    throw std::runtime_error("Mismatch in comments author_id, display_name, body and created_at size");
  }

  comments.reserve(id.size());

  for (std::size_t i=0; i<id.size(); i++) {
    comments.emplace_back(id[i],
                          author_id[i],
                          std::move(body[i]),
                          std::move(created_at[i]),
                          std::move(display_name[i]));
  }

  return comments;
}


struct node {
  using extra_columns = node_extra_columns;

  struct extra_info {
    const double lon;
    const double lat;
    extra_info(const pqxx_tuple &row, const extra_columns& col) :
      lon(double(row[col.longitude_col].as<int64_t>()) / global_settings::get_scale()),
      lat(double(row[col.latitude_col].as<int64_t>()) / global_settings::get_scale()) {}
  };
  static inline void write(
    output_formatter &formatter, const element_info &elem,
    const extra_info &extra, const tags_t &tags) {
    formatter.write_node(elem, extra.lon, extra.lat, tags);
  }
};

struct way {
  using extra_columns = way_extra_columns;

  struct extra_info {
    const nodes_t way_nodes;
    extra_info(const pqxx_tuple &row, const extra_columns& col) :
      way_nodes(extract_way_nodes(row, col)) {}
  };

  static inline void write(
    output_formatter &formatter, const element_info &elem,
    const extra_info &extra, const tags_t &tags) {
    formatter.write_way(elem, extra.way_nodes, tags);
  }
};

struct relation {
  using extra_columns = relation_extra_columns;

  struct extra_info {
    const members_t members;
    extra_info(const pqxx_tuple &row, const extra_columns& col) :
      members(extract_members(row, col)) {}

  };

  static inline void write(
    output_formatter &formatter, const element_info &elem,
    const extra_info &extra, const tags_t &tags) {
    formatter.write_relation(elem, extra.members, tags);
  }
};

template <typename T>
void extract(
  const pqxx::result &rows, output_formatter &formatter,
  std::map<osm_changeset_id_t, changeset> &cc) {

  const typename T::extra_columns extra_cols(rows);
  const elem_columns elem_cols(rows);
  const tag_columns tag_cols(rows);

  for (const auto &row : rows) {
    typename T::extra_info extra(row, extra_cols);
    auto elem = extract_elem(row, cc, elem_cols);
    auto tags = extract_tags(row, tag_cols);
    T::write(formatter, elem, extra, tags);
  }
}

} // anonymous namespace

void extract_nodes(
  const pqxx::result &rows, output_formatter &formatter,
  std::map<osm_changeset_id_t, changeset> &cc) {
  extract<node>(rows, formatter, cc);
}

void extract_ways(
  const pqxx::result &rows, output_formatter &formatter,
  std::map<osm_changeset_id_t, changeset> &cc) {
  extract<way>(rows, formatter, cc);
}

// extract relations from the results of the query and write them to the
// formatter. the changeset cache is used to look up user display names.
void extract_relations(
  const pqxx::result &rows, output_formatter &formatter,
  std::map<osm_changeset_id_t, changeset> &cc) {
  extract<relation>(rows, formatter, cc);
}

void extract_changesets(
  const pqxx::result &rows, output_formatter &formatter,
  std::map<osm_changeset_id_t, changeset> &cc,
  const std::chrono::system_clock::time_point &now,
  bool include_changeset_discussions) {

  const changeset_columns changeset_cols(rows);
  const comments_columns comments_cols(rows);
  const tag_columns tag_cols(rows);

  for (const auto &row : rows) {
    auto elem = extract_changeset(row, cc, changeset_cols);
    auto tags = extract_tags(row, tag_cols);
    auto comments = extract_comments(row, comments_cols);
    elem.comments_count = comments.size();
    formatter.write_changeset(
      elem, tags, include_changeset_discussions, comments, now);
  }
}


