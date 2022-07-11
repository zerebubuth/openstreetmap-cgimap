#include "cgimap/backend/apidb/common_pgsql_selection.hpp"
#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/backend/apidb/utils.hpp"

#include <chrono>


namespace {

using pqxx_tuple = pqxx::result::reference;
using pqxx_field = pqxx::field;

void extract_elem(const pqxx_tuple &row, element_info &elem,
                  std::map<osm_changeset_id_t, changeset> &changeset_cache) {

  elem.id        = row["id"].as<osm_nwr_id_t>();
  elem.version   = row["version"].as<int>();
  elem.timestamp = row["timestamp"].c_str();
  elem.changeset = row["changeset_id"].as<osm_changeset_id_t>();
  elem.visible   = row["visible"].as<bool>();

  changeset & cs = changeset_cache[elem.changeset];

  if (cs.data_public) {
    elem.uid = cs.user_id;
    elem.display_name = cs.display_name;
  } else {
    elem.uid = {};
    elem.display_name = {};
  }
}

template <typename T>
std::optional<T> extract_optional(const pqxx_field &f) {
  if (f.is_null()) {
    return {};
  } else {
    return f.as<T>();
  }
}

void extract_changeset(const pqxx_tuple &row,
                       changeset_info &elem,
                       std::map<osm_changeset_id_t, changeset> &changeset_cache) {
  elem.id = row["id"].as<osm_changeset_id_t>();
  elem.created_at = row["created_at"].c_str();
  elem.closed_at = row["closed_at"].c_str();

  const auto & cs = changeset_cache[elem.id];

  if (cs.data_public) {
    elem.uid = cs.user_id;
    elem.display_name = cs.display_name;
  } else {
    elem.uid = {};
    elem.display_name = {};
  }

  auto min_lat = extract_optional<int64_t>(row["min_lat"]);
  auto max_lat = extract_optional<int64_t>(row["max_lat"]);
  auto min_lon = extract_optional<int64_t>(row["min_lon"]);
  auto max_lon = extract_optional<int64_t>(row["max_lon"]);

  if (bool(min_lat) && bool(min_lon) && bool(max_lat) && bool(max_lon)) {
    elem.bounding_box = bbox(double(*min_lat) / global_settings::get_scale(),
                             double(*min_lon) / global_settings::get_scale(),
                             double(*max_lat) / global_settings::get_scale(),
                             double(*max_lon) / global_settings::get_scale());
  } else {
    elem.bounding_box = {};
  }

  elem.num_changes = row["num_changes"].as<size_t>();
}

void extract_tags(const pqxx_tuple &row, tags_t &tags) {
  tags.clear();

  auto keys   = psql_array_to_vector(row["tag_k"].as_array());
  auto values = psql_array_to_vector(row["tag_v"].as_array());

  if (keys.size()!=values.size()) {
    throw std::runtime_error("Mismatch in tags key and value size");
  }

  for(std::size_t i=0; i<keys.size(); i++)
     tags.push_back(std::make_pair(keys[i], values[i]));
}

void extract_nodes(const pqxx_tuple &row, nodes_t &nodes) {
  nodes.clear();
  auto ids = psql_array_to_vector(row["node_ids"].as_array());
  for (const auto & id : ids)
    nodes.push_back(std::stol(id));
}

element_type type_from_name(const char *name) {
  element_type type;

  switch (name[0]) {
  case 'N':
  case 'n':
    type = element_type_node;
    break;

  case 'W':
  case 'w':
    type = element_type_way;
    break;

  case 'R':
  case 'r':
    type = element_type_relation;
    break;

  default:
    // in case the name match isn't exhaustive...
    throw std::runtime_error(
        "Unexpected name not matched to type in type_from_name().");
  }

  return type;
}

void extract_members(const pqxx_tuple &row, members_t &members) {
  member_info member;
  members.clear();

  auto types = psql_array_to_vector(row["member_types"].as_array());
  auto ids   = psql_array_to_vector(row["member_ids"].as_array());
  auto roles = psql_array_to_vector(row["member_roles"].as_array());

  if (types.size()!=ids.size() || ids.size()!=roles.size()) {
    throw std::runtime_error("Mismatch in members types, ids and roles size");
  }

  for (std::size_t i=0; i<ids.size(); i++) {
    member.type = type_from_name(types[i].c_str());
    member.ref = std::stol(ids[i]);
    member.role = roles[i];
    members.push_back(member);
  }
}

void extract_comments(const pqxx_tuple &row, comments_t &comments) {
  changeset_comment_info comment;
  comments.clear();

  auto author_id    = psql_array_to_vector(row["comment_author_id"].as_array());
  auto display_name = psql_array_to_vector(row["comment_display_name"].as_array());
  auto body         = psql_array_to_vector(row["comment_body"].as_array());
  auto created_at   = psql_array_to_vector(row["comment_created_at"].as_array());

  if (author_id.size()!=display_name.size() || display_name.size()!=body.size()
      || body.size()!=created_at.size()) {
    throw std::runtime_error("Mismatch in comments author_id, display_name, body and created_at size");
  }

  for (std::size_t i=0; i<author_id.size(); i++) {
    comment.author_id = std::stol(author_id[i]);
    comment.author_display_name = display_name[i];
    comment.body = body[i];
    comment.created_at = created_at[i];
    comments.push_back(comment);
  }
}

struct node {
  struct extra_info {
    double lon, lat;
    inline void extract(const pqxx_tuple &row) {
      lon = double(row["longitude"].as<int64_t>()) / (global_settings::get_scale());
      lat = double(row["latitude"].as<int64_t>()) / (global_settings::get_scale());
    }
  };
  static inline void write(
    output_formatter &formatter, const element_info &elem,
    const extra_info &extra, const tags_t &tags) {
    formatter.write_node(elem, extra.lon, extra.lat, tags);
  }
};

struct way {
  struct extra_info {
    nodes_t nodes;
    inline void extract(const pqxx_tuple &row) {
      extract_nodes(row, nodes);
    }
  };
  static inline void write(
    output_formatter &formatter, const element_info &elem,
    const extra_info &extra, const tags_t &tags) {
    formatter.write_way(elem, extra.nodes, tags);
  }
};

struct relation {
  struct extra_info {
    members_t members;
    inline void extract(const pqxx_tuple &row) {
      extract_members(row, members);
    }
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
  std::function<void(const element_info&)> notify,
  std::map<osm_changeset_id_t, changeset> &cc) {

  element_info elem;
  typename T::extra_info extra;
  tags_t tags;

  for (const auto &row : rows) {
    extract_elem(row, elem, cc);
    extra.extract(row);
    extract_tags(row, tags);
    notify(elem);     // let callback function know about a new element we're processing
    T::write(formatter, elem, extra, tags);
  }
}

} // anonymous namespace

void extract_nodes(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  std::map<osm_changeset_id_t, changeset> &cc) {
  extract<node>(rows, formatter, notify, cc);
}

void extract_ways(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  std::map<osm_changeset_id_t, changeset> &cc) {
  extract<way>(rows, formatter, notify, cc);
}

// extract relations from the results of the query and write them to the
// formatter. the changeset cache is used to look up user display names.
void extract_relations(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  std::map<osm_changeset_id_t, changeset> &cc) {
  extract<relation>(rows, formatter, notify, cc);
}

void extract_changesets(
  const pqxx::result &rows, output_formatter &formatter,
  std::map<osm_changeset_id_t, changeset> &cc, const std::chrono::system_clock::time_point &now,
  bool include_changeset_discussions) {

  changeset_info elem;
  tags_t tags;
  comments_t comments;

  for (const auto &row : rows) {
    extract_changeset(row, elem, cc);
    extract_tags(row, tags);
    extract_comments(row, comments);
    elem.comments_count = comments.size();
    formatter.write_changeset(
      elem, tags, include_changeset_discussions, comments, now);
  }
}
