#include "cgimap/backend/apidb/common_pgsql_selection.hpp"
#include "cgimap/backend/apidb/apidb.hpp"

#include <chrono>

using std::shared_ptr;

namespace {

using pqxx_tuple = pqxx::result::reference;
using pqxx_field = pqxx::field;

void extract_elem(const pqxx_tuple &row, element_info &elem,
                  cache<osm_changeset_id_t, changeset> &changeset_cache) {

  elem.id        = row["id"].as<osm_nwr_id_t>();
  elem.version   = row["version"].as<int>();
  elem.timestamp = row["timestamp"].c_str();
  elem.changeset = row["changeset_id"].as<osm_changeset_id_t>();
  elem.visible   = row["visible"].as<bool>();

  auto cs = changeset_cache.get(elem.changeset);

  if (cs->data_public) {
    elem.uid = cs->user_id;
    elem.display_name = cs->display_name;
  } else {
    elem.uid = boost::none;
    elem.display_name = boost::none;
  }
}

template <typename T>
boost::optional<T> extract_optional(const pqxx_field &f) {
  if (f.is_null()) {
    return boost::none;
  } else {
    return f.as<T>();
  }
}

void extract_changeset(const pqxx_tuple &row,
                       changeset_info &elem,
                       cache<osm_changeset_id_t, changeset> &changeset_cache) {
  elem.id = row["id"].as<osm_changeset_id_t>();
  elem.created_at = row["created_at"].c_str();
  elem.closed_at = row["closed_at"].c_str();

  auto cs = changeset_cache.get(elem.id);

  if (cs->data_public) {
    elem.uid = cs->user_id;
    elem.display_name = cs->display_name;
  } else {
    elem.uid = boost::none;
    elem.display_name = boost::none;
  }

  auto min_lat = extract_optional<int64_t>(row["min_lat"]);
  auto max_lat = extract_optional<int64_t>(row["max_lat"]);
  auto min_lon = extract_optional<int64_t>(row["min_lon"]);
  auto max_lon = extract_optional<int64_t>(row["max_lon"]);

  if (bool(min_lat) && bool(min_lon) && bool(max_lat) && bool(max_lon)) {
    elem.bounding_box = bbox(double(*min_lat) / SCALE,
                             double(*min_lon) / SCALE,
                             double(*max_lat) / SCALE,
                             double(*max_lon) / SCALE);
  } else {
    elem.bounding_box = boost::none;
  }

  elem.num_changes = row["num_changes"].as<size_t>();
}

void extract_tags(const pqxx_tuple &row, tags_t &tags) {
  tags.clear();

  auto keys   = psql_array_to_vector(row["tag_k"].c_str());
  auto values = psql_array_to_vector(row["tag_v"].c_str());

  if (keys.size()!=values.size()) {
    throw std::runtime_error("Mismatch in tags key and value size");
  }

  for(int i=0; i<keys.size(); i++)
     tags.push_back(std::make_pair(keys[i], values[i]));
}

void extract_nodes(const pqxx_tuple &row, nodes_t &nodes) {
  nodes.clear();
  auto ids = psql_array_to_vector(row["node_ids"].c_str());
  for (const auto & id : ids)
    nodes.push_back(boost::lexical_cast<osm_nwr_id_t>(id));
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

  auto types = psql_array_to_vector(row["member_types"].c_str());
  auto ids   = psql_array_to_vector(row["member_ids"].c_str());
  auto roles = psql_array_to_vector(row["member_roles"].c_str());

  if (types.size()!=ids.size() || ids.size()!=roles.size()) {
    throw std::runtime_error("Mismatch in members types, ids and roles size");
  }

  for (int i=0; i<ids.size(); i++) {
    member.type = type_from_name(types[i].c_str());
    member.ref = boost::lexical_cast<osm_nwr_id_t>(ids[i]);
    member.role = roles[i];
    members.push_back(member);
  }
}

void extract_comments(const pqxx_tuple &row, comments_t &comments) {
  changeset_comment_info comment;
  comments.clear();

  auto author_id    = psql_array_to_vector(row["comment_author_id"].c_str());
  auto display_name = psql_array_to_vector(row["comment_display_name"].c_str());
  auto body         = psql_array_to_vector(row["comment_body"].c_str());
  auto created_at   = psql_array_to_vector(row["comment_created_at"].c_str());

  if (author_id.size()!=display_name.size() || display_name.size()!=body.size()
      || body.size()!=created_at.size()) {
    throw std::runtime_error("Mismatch in comments author_id, display_name, body and created_at size");
  }

  for (int i=0; i<author_id.size(); i++) {
    comment.author_id = boost::lexical_cast<osm_nwr_id_t>(author_id[i]);
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
      lon = double(row["longitude"].as<int64_t>()) / (SCALE);
      lat = double(row["latitude"].as<int64_t>()) / (SCALE);
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
  cache<osm_changeset_id_t, changeset> &cc) {

  element_info elem;
  typename T::extra_info extra;
  tags_t tags;

  std::set<osm_changeset_id_t> changeset_ids;

  for (const auto &row : rows)
    changeset_ids.insert(row["changeset_id"].as<osm_changeset_id_t>());

  cc.prefetch(changeset_ids);

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
  cache<osm_changeset_id_t, changeset> &cc) {
  extract<node>(rows, formatter, notify, cc);
}

void extract_ways(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  cache<osm_changeset_id_t, changeset> &cc) {
  extract<way>(rows, formatter, notify, cc);
}

// extract relations from the results of the query and write them to the
// formatter. the changeset cache is used to look up user display names.
void extract_relations(
  const pqxx::result &rows, output_formatter &formatter,
  std::function<void(const element_info&)> notify,
  cache<osm_changeset_id_t, changeset> &cc) {
  extract<relation>(rows, formatter, notify, cc);
}

void extract_changesets(
  const pqxx::result &rows, output_formatter &formatter,
  cache<osm_changeset_id_t, changeset> &cc, const std::chrono::system_clock::time_point &now,
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
