/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#include "xmlparser.hpp"

#include <istream>
#include <ostream>
#include <ios>
#include <string_view>
#include <unordered_set>
#include <optional>

#include <fmt/core.h>
#include <libxml/parser.h>

namespace xmlparser {

template <typename T>
std::optional<T> opt_attribute(std::string_view name, const xmlChar **attributes) {
  while (*attributes != nullptr) {
    if (std::string_view(reinterpret_cast<const char*>(*attributes++)) == name) {
      std::string_view val(reinterpret_cast<const char*>(*attributes));
      if constexpr (std::is_same_v<T, bool>) {
        return {val == "true"};
      } else if constexpr (std::is_integral_v<T>) {
        return {std::stol(reinterpret_cast<const char*>(*attributes))};
      } else if constexpr (std::is_floating_point_v<T>) {
        return {std::stod(reinterpret_cast<const char*>(*attributes))};
      } else {
        return {std::string(val)};
      }
    }
    ++attributes;
  }
  return {};
}

template <typename T>
T get_attribute(std::string_view name, const xmlChar **attributes) {
  auto res = opt_attribute<T>(name, attributes);
  if (res)
    return *res;

  throw std::runtime_error(fmt::format("Unable to find attribute {}.", name));
}


void parse_info(element_info &info, const xmlChar **attributes) {
  info.id = get_attribute<osm_nwr_id_t>("id", attributes);
  info.version = get_attribute<osm_nwr_id_t>("version", attributes);
  info.changeset = get_attribute<osm_changeset_id_t>("changeset", attributes);
  info.timestamp = get_attribute<std::string>("timestamp", attributes);
  info.uid = opt_attribute<osm_user_id_t>("uid", attributes);
  info.display_name = opt_attribute<std::string>("user", attributes);
  info.visible = get_attribute<bool>("visible", attributes);
  info.redaction = opt_attribute<osm_redaction_id_t>("redaction", attributes);
}

void parse_changeset_info(changeset_info &info, const xmlChar **attributes) {
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

struct xml_parser {
  explicit xml_parser(xmlparser::database *db)
    : m_db(db) {}

  static void start_element(void *ctx, const xmlChar *name_cstr,
                            const xmlChar **attributes) {
    auto *parser = static_cast<xml_parser *>(ctx);

    std::string_view name((const char *)name_cstr);

    if (name == "node") {
      node n;
      parse_info(n.m_info, attributes);
      n.m_lon = get_attribute<double>("lon", attributes);
      n.m_lat = get_attribute<double>("lat", attributes);
      id_version idv(n.m_info.id, n.m_info.version);
      auto status = parser->m_db->m_nodes.insert(std::make_pair(idv, n));
      parser->m_cur_node = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_node->m_tags);
      parser->m_cur_way = nullptr;
      parser->m_cur_rel = nullptr;
      parser->m_cur_changeset = nullptr;
    }
    else if (name == "way") {
      way w;
      parse_info(w.m_info, attributes);
      id_version idv(w.m_info.id, w.m_info.version);
      auto status = parser->m_db->m_ways.insert(std::make_pair(idv, w));
      parser->m_cur_way = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_way->m_tags);
      parser->m_cur_node = nullptr;
      parser->m_cur_rel = nullptr;
      parser->m_cur_changeset = nullptr;
    }
    else if (name == "relation") {
      relation r;
      parse_info(r.m_info, attributes);
      id_version idv(r.m_info.id, r.m_info.version);
      auto status = parser->m_db->m_relations.insert(std::make_pair(idv, r));
      parser->m_cur_rel = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_rel->m_tags);
      parser->m_cur_node = nullptr;
      parser->m_cur_way = nullptr;
      parser->m_cur_changeset = nullptr;
    }
    else if (name == "changeset") {
      changeset c;
      parse_changeset_info(c.m_info, attributes);
      auto status = parser->m_db->m_changesets.insert(std::make_pair(c.m_info.id, c));
      parser->m_cur_changeset = &(status.first->second);
      parser->m_cur_tags = &(parser->m_cur_changeset->m_tags);
      parser->m_cur_node = nullptr;
      parser->m_cur_way = nullptr;
      parser->m_cur_rel = nullptr;
    }
    else if (name == "tag") {
      if (parser->m_cur_tags != nullptr) {
        auto k = get_attribute<std::string>("k", attributes);
        auto v = get_attribute<std::string>("v", attributes);
        parser->m_cur_tags->push_back(std::make_pair(k, v));
      }
    }
    else if (name == "nd") {
      if (parser->m_cur_way != nullptr) {
        parser->m_cur_way->m_nodes.push_back(
            get_attribute<osm_nwr_id_t>("ref", attributes));
      }
    }
    else if (name == "member") {
      if (parser->m_cur_rel != nullptr) {
        member_info m;
        auto member_type = get_attribute<std::string>("type", attributes);
        if (member_type == "node") {
          m.type = element_type::node;
        } else if (member_type == "way") {
          m.type = element_type::way;
        } else if (member_type == "relation") {
          m.type = element_type::relation;
        } else {
          throw std::runtime_error(fmt::format("Unknown member type `{}'.", member_type));
        }
        m.ref = get_attribute<osm_nwr_id_t>("ref", attributes);
        m.role = get_attribute<std::string>("role", attributes);
        parser->m_cur_rel->m_members.push_back(m);
      }
    }
    else if (name == "comment") {
      if (parser->m_cur_changeset != nullptr) {
        parser->m_cur_changeset->m_info.comments_count++;
        changeset_comment_info info;
        info.id = get_attribute<osm_changeset_comment_id_t>("id", attributes);
        info.author_id = get_attribute<osm_user_id_t>("uid", attributes);
        info.author_display_name = get_attribute<std::string>("user", attributes);
        info.created_at = get_attribute<std::string>("date", attributes);
        parser->m_cur_changeset->m_comments.push_back(info);
      }
    }
    else if (name == "text") {
      if ((parser->m_cur_changeset != nullptr) &&
          (!parser->m_cur_changeset->m_comments.empty())) {
        parser->m_in_text = true;
      }
    }
  }

  static void end_element(void *ctx, const xmlChar *) {
    auto *parser = static_cast<xml_parser *>(ctx);
    parser->m_in_text = false;
  }

  static void characters(void *ctx, const xmlChar *str, int len) {
    auto *parser = static_cast<xml_parser *>(ctx);

    if (parser->m_in_text) {
      parser->m_cur_changeset->m_comments.back().body.append((const char *)str, len);
    }
  }

  static void warning(void *, const char *, ...) {
    // TODO
  }

  static void error(void *, const char *fmt, ...) {
    char buffer[1024];
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vsnprintf(buffer, sizeof(buffer) - 1, fmt, arg_ptr);
    va_end(arg_ptr);
    throw std::runtime_error(fmt::format("XML ERROR: {}", buffer));
  }

  xmlparser::database *m_db = nullptr;
  xmlparser::node *m_cur_node = nullptr;
  xmlparser::way *m_cur_way = nullptr;
  xmlparser::relation *m_cur_rel = nullptr;
  tags_t *m_cur_tags = nullptr;
  xmlparser::changeset *m_cur_changeset = nullptr;
  bool m_in_text = false;
};

xmlSAXHandler create_xml_sax_handler() {
  xmlSAXHandler handler = {};
  handler.startElement = &xml_parser::start_element;
  handler.endElement = &xml_parser::end_element;
  handler.characters = &xml_parser::characters;
  handler.warning = &xml_parser::warning;
  handler.error = &xml_parser::error;
  handler.initialized = XML_SAX2_MAGIC;
  return handler;
}

} // namespace xmlparser

std::unique_ptr<xmlparser::database> parse_xml(const char *filename) {
  xmlSAXHandler handler = xmlparser::create_xml_sax_handler();
  auto db = std::make_unique<xmlparser::database>();
  xmlparser::xml_parser parser(db.get());
  int status = xmlSAXUserParseFile(&handler, &parser, filename);
  if (status != 0) {
    const auto err = xmlGetLastError();
    throw std::runtime_error(
        fmt::format("XML ERROR: {}.", err->message));
  }

  xmlCleanupParser();

  return db;
}

std::unique_ptr<xmlparser::database> parse_xml_from_string(const std::string &payload) {
  xmlSAXHandler handler = xmlparser::create_xml_sax_handler();
  auto db = std::make_unique<xmlparser::database>();
  xmlparser::xml_parser parser(db.get());
  int status = xmlSAXUserParseMemory(&handler, &parser, payload.c_str(), payload.size());
  if (status != 0) {
    const auto err = xmlGetLastError();
    throw std::runtime_error(
        fmt::format("XML ERROR: {}.", err->message));
  }

  xmlCleanupParser();

  return db;
}