/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSMCHANGE_JSON_INPUT_FORMAT_HPP
#define OSMCHANGE_JSON_INPUT_FORMAT_HPP

#include "cgimap/api06/changeset_upload/node.hpp"
#include "cgimap/api06/changeset_upload/osmobject.hpp"
#include "cgimap/api06/changeset_upload/parser_callback.hpp"
#include "cgimap/api06/changeset_upload/relation.hpp"
#include "cgimap/api06/changeset_upload/way.hpp"
#include "cgimap/types.hpp"

#include "sjparser/sjparser.h"

#include <fmt/core.h>

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>


namespace api06 {

using SJParser::Array;
using SJParser::Member;
using SJParser::Object;
using SJParser::Parser;
using SJParser::Presence;
using SJParser::SArray;
using SJParser::SAutoObject;
using SJParser::SMap;
using SJParser::Value;
using SJParser::Reaction;
using SJParser::ObjectOptions;
using SJParser::Presence::Optional;

class OSMChangeJSONParserFormat {

  static auto getMemberParser() {

    return SAutoObject{std::tuple{Member{"type", Value<std::string>{}},
                                  Member{"ref", Value<int64_t>{}},
                                  Member{"role", Value<std::string>{}, Optional, ""}},
                                  ObjectOptions{Reaction::Ignore}
                                  };
  }

  template <typename ElementParserCallback = std::nullptr_t>
  static auto getElementsParser(ElementParserCallback element_parser_callback = nullptr) {
    return Object{
          std::tuple{
            Member{"type", Value<std::string>{}},
            Member{"action", Value<std::string>{}},
            Member{"if-unused", Value<bool>{}, Optional, false},
            Member{"id", Value<int64_t>{}},
            Member{"lat", Value<double>{}, Optional},
            Member{"lon", Value<double>{}, Optional},
            Member{"version", Value<int64_t>{}, Optional},
            Member{"changeset", Value<int64_t>{}},
            Member{"tags", SMap{Value<std::string>{}}, Optional},
            Member{"nodes", SArray{Value<int64_t>{}}, Optional},
            Member{"members", SArray{getMemberParser()}, Optional}
          },
        ObjectOptions{Reaction::Ignore},
        element_parser_callback};
  }

  template <typename ElementParserCallback = std::nullptr_t,
            typename CheckVersionCallback = std::nullptr_t>
  static auto getMainParser(CheckVersionCallback check_version_callback = nullptr,
                            ElementParserCallback element_parser_callback = nullptr) {
    return Parser{
       Object{
        std::tuple{
          Member{"version", Value<std::string>{check_version_callback}},
          Member{"generator", Value<std::string>{}, Optional},
          Member{"osmChange",  Array{getElementsParser(element_parser_callback)}}
        },ObjectOptions{Reaction::Ignore}}};
  }

  friend class OSMChangeJSONParser;
};

class OSMChangeJSONParser {

public:
  explicit OSMChangeJSONParser(Parser_Callback& callback)
      : m_callback(callback) { }

  OSMChangeJSONParser(const OSMChangeJSONParser &) = delete;
  OSMChangeJSONParser &operator=(const OSMChangeJSONParser &) = delete;

  OSMChangeJSONParser(OSMChangeJSONParser &&) = delete;
  OSMChangeJSONParser &operator=(OSMChangeJSONParser &&) = delete;

  void process_message(const std::string &data) {

    try {
      m_callback.start_document();
      _parser.parse(data);
      _parser.finish();

      if (_parser.parser().isEmpty()) {
        throw payload_error("Empty JSON payload");
      }

      if (element_count == 0) {
        throw payload_error("osmChange array is empty");
      }

      m_callback.end_document();
    } catch (const std::exception& e) {
      throw http::bad_request(e.what());    // rethrow JSON parser error as HTTP 400 Bad request
    }
  }

private:

  using ElementsParser = decltype(api06::OSMChangeJSONParserFormat::getElementsParser());
  using MainParser = decltype(api06::OSMChangeJSONParserFormat::getMainParser());

  MainParser _parser{api06::OSMChangeJSONParserFormat::getMainParser(
                    std::bind_front(&api06::OSMChangeJSONParser::check_version_callback, this),
                    std::bind_front(&api06::OSMChangeJSONParser::process_element, this))};

  bool check_version_callback(const std::string& version) const {

    if (version != "0.6") {
      throw payload_error{fmt::format(R"(Unsupported version "{}", expecting "0.6")", version)};
    }
    return true;
  }

  // OSM element callback
  bool process_element(ElementsParser &parser) {

    element_count++;

    // process action
    process_action(parser);

    // process if-unused flag for delete action
    process_if_unused(parser);

    // process type (node, way, relation)
    process_type(parser);

    return true;
  }

  void process_action(ElementsParser &parser) {

    using enum operation;
    const std::string& action = parser.get<1>();

    if (action == "create") {
      m_operation = op_create;
    } else if (action == "modify") {
      m_operation = op_modify;
    } else if (action == "delete") {
      m_operation = op_delete;
    } else {
      throw payload_error{fmt::format("Unknown action {}, choices are create, modify, delete", action)};
    }
  }

  void process_if_unused(ElementsParser &parser) {

    if (m_operation == operation::op_delete) {
      m_if_unused = false;
      if (parser.parser<2>().isSet()) {
        m_if_unused = parser.get<2>();
      }
    }
    else {
      m_if_unused = false;
      if (parser.parser<2>().isSet()) {
        throw payload_error{fmt::format("if-unused attribute is not allowed for {} action", parser.get<1>())};
      }
    }
  }

  void process_type(ElementsParser &parser) {

    const std::string& type = parser.get<0>();

    if (type == "node") {
      process_node(parser);
    } else if (type == "way") {
      process_way(parser);
    } else if (type == "relation") {
      process_relation(parser);
    } else {
      throw payload_error{fmt::format("Unknown element {}, expecting node, way or relation", type)};
    }
  }

  void process_node(ElementsParser& parser) {

    if (parser.parser<9>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has way nodes, but it is not a way", parser.get<0>(), parser.get<3>())};
    }

    if (parser.parser<10>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has relation members, but it is not a relation", parser.get<0>(), parser.get<3>())};
    }

    Node node;
    init_object(node, parser);

    if (parser.parser<4>().isSet()) {
      node.set_lat(parser.get<4>());
    }

    if (parser.parser<5>().isSet()) {
      node.set_lon(parser.get<5>());
    }

    process_tags(node, parser);

    if (!node.is_valid(m_operation)) {
      throw payload_error{fmt::format("{} does not include all mandatory fields", node.to_string())};
    }

    m_callback.process_node(node, m_operation, m_if_unused);
  }

  void process_way(ElementsParser& parser) {

    if (parser.parser<4>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has lat, but it is not a node", parser.get<0>(), parser.get<3>())};
    }

    if (parser.parser<5>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has lon, but it is not a node", parser.get<0>(), parser.get<3>())};
    }

    if (parser.parser<10>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has relation members, but it is not a relation", parser.get<0>(), parser.get<3>())};
    }

    Way way;
    init_object(way, parser);

    // adding way nodes
    if (parser.parser<9>().isSet()) {
      for (const auto& way_node_id : parser.get<9>()) {
          way.add_way_node(way_node_id);
      }
    }

    process_tags(way, parser);

    if (!way.is_valid(m_operation)) {
      throw payload_error{fmt::format("{} does not include all mandatory fields", way.to_string())};
    }

    m_callback.process_way(way, m_operation, m_if_unused);
  }

  void process_relation(ElementsParser& parser) {

    if (parser.parser<4>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has lat, but it is not a node", parser.get<0>(), parser.get<3>())};
    }

    if (parser.parser<5>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has lon, but it is not a node", parser.get<0>(), parser.get<3>())};
    }

    if (parser.parser<9>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has way nodes, but it is not a way", parser.get<0>(), parser.get<3>())};
    }

    Relation relation;
    init_object(relation, parser);

    process_relation_members(relation, parser);

    process_tags(relation, parser);

    if (!relation.is_valid(m_operation)) {
      throw payload_error{fmt::format("{} does not include all mandatory fields", relation.to_string())};
    }

    m_callback.process_relation(relation, m_operation, m_if_unused);
  }

  void process_relation_members(Relation &relation, ElementsParser& parser) const {

    // relation member attribute is mandatory for create and modify operations.
    // Its value can be an empty array, though. Delete operations don't require
    // this attribute.
    if (m_operation == operation::op_delete)
      return;

    if (!parser.parser<10>().isSet()) {
      throw payload_error{fmt::format("Element {}/{:d} has no relation member attribute", parser.get<0>(), parser.get<3>())};
    }

    for (const auto& [type, ref, role] : parser.get<10>()) {
      RelationMember member;
      member.set_type(type);
      member.set_ref(ref);
      member.set_role(role);

      if (!member.is_valid()) {
        throw payload_error{fmt::format("Missing mandatory field on relation member in {}", relation.to_string()) };
      }
      relation.add_member(member);
    }
  }

  void process_tags(OSMObject &o, ElementsParser& parser) const {

    if (parser.parser<8>().isSet()) {
      for (const auto &[key, value] : parser.get<8>()) {
         o.add_tag(key, value);
      }
    }
  }

  void init_object(OSMObject &object, ElementsParser& parser) const {

    // id
    object.set_id(parser.get<3>());

    // version
    if (parser.parser<6>().isSet()) {
      object.set_version(parser.get<6>());
    }

    // changeset
    if (parser.parser<7>().isSet()) {
      object.set_changeset(parser.get<7>());
    }

    // TODO: not needed, handled by sjparser
    if (!object.has_id()) {
     	throw payload_error{ "Mandatory field id missing in object" };
    }

    if (!object.has_changeset()) {
      throw payload_error{fmt::format("Changeset id is missing for {}", object.to_string()) };
    }

    if (m_operation == operation::op_create) {
      // we always override version number for create operations (they are not
      // mandatory)
      object.set_version(0u);
    } else if (m_operation == operation::op_delete ||
               m_operation == operation::op_modify) {
      // objects for other operations must have a positive version number
      if (!object.has_version()) {
        throw payload_error{fmt::format("Version is required when updating {}", object.to_string()) };
      }
      if (object.version() < 1) {
        throw payload_error{ fmt::format("Invalid version number {} in {}", object.version(), object.to_string()) };
      }
    }
  }

  operation m_operation = operation::op_undefined;
  Parser_Callback& m_callback;
  bool m_if_unused = false;
  int element_count = 0;
};

} // namespace api06

#endif // OSMCHANGE_JSON_INPUT_FORMAT_HPP
