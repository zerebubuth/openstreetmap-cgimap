/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "xmlparser.hpp"
#include "parsers/saxparser.hpp"

#include <fstream>
#include <optional>
#include <string_view>

#include <fmt/core.h>
#include <libxml/parser.h>

namespace xmlparser {

struct xml_parser : private xmlpp::SaxParser {
  explicit xml_parser(xmlparser::database *db) : m_db(db) {}

  void process_message(const std::string &data) {

    try {
      parse_memory(data);
    } catch (const xmlpp::exception &e) {
      throw std::runtime_error(e.what());
    }
  }

protected:
  void on_start_element(const char *elem, const char **attributes) override {

    std::string_view name((const char *)elem);

    if (name == "node") {
      extract_node(attributes);
    } else if (name == "way") {
      extract_way(attributes);
    } else if (name == "relation") {
      extract_relation(attributes);
    } else if (name == "changeset") {
      extract_changeset(attributes);
    } else if (name == "tag") {
      extract_tag(attributes);
    } else if (name == "nd") {
      extract_way_node(attributes);
    } else if (name == "member") {
      extract_relation_member(attributes);
    } else if (name == "comment") {
      extract_changset_comment(attributes);
    } else if (name == "text") {
      if ((m_cur_changeset != nullptr) &&
          (!m_cur_changeset->m_comments.empty())) {
        m_in_text = true;
      }
    }
  }

  void on_end_element(const char *elem) override { m_in_text = false; }

  void on_characters(const std::string &characters) override {

    if (m_in_text) {
      m_cur_changeset->m_comments.back().body.append(characters);
    }
  }

private:
  void extract_node(const char **&attributes) {
    node n;
    parse_info(n.m_info, attributes);
    n.m_lon = get_attribute<double>("lon", attributes);
    n.m_lat = get_attribute<double>("lat", attributes);
    id_version idv(n.m_info.id, n.m_info.version);
    auto status = m_db->m_nodes.insert({idv, n});
    m_cur_node = &(status.first->second);
    m_cur_tags = &(m_cur_node->m_tags);
    m_cur_way = nullptr;
    m_cur_rel = nullptr;
    m_cur_changeset = nullptr;
  }

  void extract_way(const char **&attributes) {
    way w;
    parse_info(w.m_info, attributes);
    id_version idv(w.m_info.id, w.m_info.version);
    auto status = m_db->m_ways.insert({idv, w});
    m_cur_way = &(status.first->second);
    m_cur_tags = &(m_cur_way->m_tags);
    m_cur_node = nullptr;
    m_cur_rel = nullptr;
    m_cur_changeset = nullptr;
  }

  void extract_way_node(const char **&attributes) {
    if (m_cur_way != nullptr) {
      m_cur_way->m_nodes.push_back(
          get_attribute<osm_nwr_id_t>("ref", attributes));
    }
  }

  void extract_relation(const char **&attributes) {
    relation r;
    parse_info(r.m_info, attributes);
    id_version idv(r.m_info.id, r.m_info.version);
    auto status = m_db->m_relations.insert({idv, r});
    m_cur_rel = &(status.first->second);
    m_cur_tags = &(m_cur_rel->m_tags);
    m_cur_node = nullptr;
    m_cur_way = nullptr;
    m_cur_changeset = nullptr;
  }

  void extract_relation_member(const char **&attributes) {
    if (m_cur_rel != nullptr) {
      member_info m;
      auto member_type = get_attribute<std::string>("type", attributes);
      if (member_type == "node") {
        m.type = element_type::node;
      } else if (member_type == "way") {
        m.type = element_type::way;
      } else if (member_type == "relation") {
        m.type = element_type::relation;
      } else {
        throw std::runtime_error(
            fmt::format("Unknown member type `{}'.", member_type));
      }
      m.ref = get_attribute<osm_nwr_id_t>("ref", attributes);
      m.role = get_attribute<std::string>("role", attributes);
      m_cur_rel->m_members.push_back(m);
    }
  }

  void extract_tag(const char **&attributes) {
    if (m_cur_tags != nullptr) {
      auto k = get_attribute<std::string>("k", attributes);
      auto v = get_attribute<std::string>("v", attributes);
      m_cur_tags->emplace_back(k, v);
    }
  }

  void extract_changeset(const char **&attributes) {
    changeset c;
    parse_changeset_info(c.m_info, attributes);
    auto status = m_db->m_changesets.insert({c.m_info.id, c});
    m_cur_changeset = &(status.first->second);
    m_cur_tags = &(m_cur_changeset->m_tags);
    m_cur_node = nullptr;
    m_cur_way = nullptr;
    m_cur_rel = nullptr;
  }

  void extract_changset_comment(const char **&attributes) {
    if (m_cur_changeset != nullptr) {
      m_cur_changeset->m_info.comments_count++;
      changeset_comment_info info;
      info.id = get_attribute<osm_changeset_comment_id_t>("id", attributes);
      info.author_id = get_attribute<osm_user_id_t>("uid", attributes);
      info.author_display_name = get_attribute<std::string>("user", attributes);
      info.created_at = get_attribute<std::string>("date", attributes);
      m_cur_changeset->m_comments.push_back(info);
    }
  }

  template <typename T>
  std::optional<T> opt_attribute(std::string_view name,
                                 const char **attributes) {
    while (*attributes != nullptr) {
      if (std::string_view(attributes[0]) == name) {
        std::string_view val(attributes[1]);
        if constexpr (std::is_same_v<T, bool>) {
          return { val == "true" };
        } else if constexpr (std::is_integral_v<T>) {
          return { std::stol(attributes[1]) };
        } else if constexpr (std::is_floating_point_v<T>) {
          return { std::stod(attributes[1]) };
        } else {
          return { std::string(val) };
        }
      }
      attributes += 2;
    }
    return {};
  }

  template <typename T>
  T get_attribute(std::string_view name, const char **attributes) {
    auto res = opt_attribute<T>(name, attributes);
    if (res)
      return *res;

    throw std::runtime_error(fmt::format("Unable to find attribute {}.", name));
  }

  void parse_info(element_info &info, const char **attributes) {
    info.id = get_attribute<osm_nwr_id_t>("id", attributes);
    info.version = get_attribute<osm_nwr_id_t>("version", attributes);
    info.changeset = get_attribute<osm_changeset_id_t>("changeset", attributes);
    info.timestamp = get_attribute<std::string>("timestamp", attributes);
    info.uid = opt_attribute<osm_user_id_t>("uid", attributes);
    info.display_name = opt_attribute<std::string>("user", attributes);
    info.visible = get_attribute<bool>("visible", attributes);
    info.redaction = opt_attribute<osm_redaction_id_t>("redaction", attributes);
  }

  void parse_changeset_info(changeset_info &info, const char **attributes) {
    info.id = get_attribute<osm_changeset_id_t>("id", attributes);
    info.created_at = get_attribute<std::string>("created_at", attributes);
    info.closed_at = get_attribute<std::string>("closed_at", attributes);
    info.uid = opt_attribute<osm_user_id_t>("uid", attributes);
    info.display_name = opt_attribute<std::string>("user", attributes);
    info.bounding_box = {};

    auto min_lat = opt_attribute<double>("min_lat", attributes);
    auto min_lon = opt_attribute<double>("min_lon", attributes);
    auto max_lat = opt_attribute<double>("max_lat", attributes);
    auto max_lon = opt_attribute<double>("max_lon", attributes);

    if (min_lat && min_lon && max_lat && max_lon) {
      info.bounding_box = bbox(*min_lat, *min_lon, *max_lat, *max_lon);
    }

    info.num_changes = get_attribute<size_t>("num_changes", attributes);
    info.comments_count = 0;
  }

  xmlparser::database *m_db = nullptr;
  xmlparser::node *m_cur_node = nullptr;
  xmlparser::way *m_cur_way = nullptr;
  xmlparser::relation *m_cur_rel = nullptr;
  tags_t *m_cur_tags = nullptr;
  xmlparser::changeset *m_cur_changeset = nullptr;
  bool m_in_text = false;
};

} // namespace xmlparser

std::unique_ptr<xmlparser::database> parse_xml(const char *filename) {

  std::ifstream t(filename);
  std::stringstream buffer;
  buffer << t.rdbuf();

  return parse_xml_from_string(buffer.str());
}

std::unique_ptr<xmlparser::database>
parse_xml_from_string(const std::string &payload) {
  auto db = std::make_unique<xmlparser::database>();
  xmlparser::xml_parser parser(db.get());
  parser.process_message(payload);
  return db;
}