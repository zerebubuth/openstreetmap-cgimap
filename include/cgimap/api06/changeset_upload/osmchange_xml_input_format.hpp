/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OSMCHANGE_XML_INPUT_FORMAT_HPP
#define OSMCHANGE_XML_INPUT_FORMAT_HPP

#include "cgimap/api06/changeset_upload/node.hpp"
#include "cgimap/api06/changeset_upload/osmobject.hpp"
#include "cgimap/api06/changeset_upload/parser_callback.hpp"
#include "cgimap/api06/changeset_upload/relation.hpp"
#include "cgimap/api06/changeset_upload/way.hpp"
#include "cgimap/types.hpp"
#include "parsers/saxparser.hpp"

#include <libxml/parser.h>
#include <fmt/core.h>

#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace api06 {

class OSMChangeXMLParser : private xmlpp::SaxParser {

public:
  explicit OSMChangeXMLParser(Parser_Callback& callback)
      : m_callback(callback) {

    m_context.push_back(context::root);
  }

  OSMChangeXMLParser(const OSMChangeXMLParser &) = delete;
  OSMChangeXMLParser &operator=(const OSMChangeXMLParser &) = delete;

  OSMChangeXMLParser(OSMChangeXMLParser &&) = delete;
  OSMChangeXMLParser &operator=(OSMChangeXMLParser &&) = delete;

  void process_message(const std::string &data) {

    try {
      parse_memory(data);
    } catch (const xmlpp::exception& e) {
      throw http::bad_request(e.what());    // rethrow XML parser error as HTTP 400 Bad request
    }
  }

protected:

  void on_start_element(const char *elem, const char **attrs) override {

    assert (!m_context.empty());

    const std::string_view element(elem);

    switch (m_context.back()) {
    case context::root:
      if (element == "osmChange") {
        m_callback.start_document();
      } else {
        throw payload_error{
          fmt::format("Unknown top-level element {}, expecting osmChange",
           element)
        };
      }
      m_context.push_back(context::top);
      break;

    case context::top:

      if (element == "create") {
        m_context.push_back(context::in_create);
        m_operation = operation::op_create;
      } else if (element == "modify") {
        m_context.push_back(context::in_modify);
        m_operation = operation::op_modify;
      } else if (element == "delete") {
        m_if_unused = false;
        check_attributes(attrs, [&](std::string_view name, const std::string & /*unused*/ ) {

          if (name == "if-unused") {
            m_if_unused = true;
          }
        });
        m_context.push_back(context::in_delete);
        m_operation = operation::op_delete;
      } else {
        throw payload_error{
           fmt::format(
               "Unknown action {}, choices are create, modify, delete",
           element)
        };
      }
      break;

    case context::in_delete:
    case context::in_create:
    case context::in_modify:

      if (element == "node") {
        m_node.reset(new Node{});
        init_object(*m_node, attrs);
        init_node(*m_node, attrs);
        m_context.push_back(context::node);
      } else if (element == "way") {
        m_way.reset(new Way{});
        init_object(*m_way, attrs);
        m_context.push_back(context::way);
      } else if (element == "relation") {
        m_relation.reset(new Relation{});
        init_object(*m_relation, attrs);
        m_context.push_back(context::relation);
      } else {
        throw payload_error{
          fmt::format(
               "Unknown element {}, expecting node, way or relation",
           element)
        };
      }
      break;

    case context::node:
      m_context.push_back(context::in_object);
      if (element == "tag") {
        add_tag(*m_node, attrs);
      }
      break;
    case context::way:
      m_context.push_back(context::in_object);
      if (element == "nd") {
	bool ref_found = false;
        check_attributes(attrs, [&](std::string_view name, const std::string &value) {
          if (name == "ref") {
            m_way->add_way_node(value);
            ref_found = true;
          }
        });
        if (!ref_found)
          throw payload_error{fmt::format(
                                "Missing mandatory ref field on way node {}",
                            m_way->to_string()) };
      } else if (element == "tag") {
        add_tag(*m_way, attrs);
      }
      break;
    case context::relation:
      m_context.push_back(context::in_object);
      if (element == "member") {
        RelationMember member;
        check_attributes(attrs, [&member](std::string_view name, const std::string &value) {
          if (name == "type") {
            member.set_type(value);
          } else if (name == "ref") {
            member.set_ref(value);
          } else if (name == "role") {
            member.set_role(value);
          }
        });
        if (!member.is_valid()) {
          throw payload_error{ fmt::format(
                                "Missing mandatory field on relation member in {}",
                            m_relation->to_string()) };
        }
        m_relation->add_member(member);
      } else if (element == "tag") {
        add_tag(*m_relation, attrs);
      }
      break;
    case context::in_object:
      throw payload_error{ "xml file nested too deep" };
      break;
    }
  }

  void on_end_element(const char *elem) override {

    assert (!m_context.empty());

    const std::string_view element(elem);

    switch (m_context.back()) {
    case context::root:
      assert(false);
      break;
    case context::top:
      assert(element == "osmChange");
      m_context.pop_back();
      m_operation = operation::op_undefined;
      m_callback.end_document();
      break;
    case context::in_create:
      assert(element == "create");
      m_operation = operation::op_undefined;
      m_context.pop_back();
      break;
    case context::in_modify:
      assert(element == "modify");
      m_operation = operation::op_undefined;
      m_context.pop_back();
      break;
    case context::in_delete:
      assert(element == "delete");
      m_operation = operation::op_undefined;
      m_context.pop_back();
      m_if_unused = false;
      break;
    case context::node:
      assert(element == "node");
      if (!m_node->is_valid(m_operation)) {
        throw payload_error{
          fmt::format("{} does not include all mandatory fields",
           m_node->to_string())
        };
      }
      m_callback.process_node(*m_node, m_operation, m_if_unused);
      m_node.reset(new Node{});
      m_context.pop_back();
      break;
    case context::way:
      assert(element == "way");
      if (!m_way->is_valid(m_operation)) {
        throw payload_error{
          fmt::format("{} does not include all mandatory fields",
           m_way->to_string())
        };
      }

      m_callback.process_way(*m_way, m_operation, m_if_unused);
      m_way.reset(new Way{});
      m_context.pop_back();
      break;
    case context::relation:
      assert(element == "relation");
      if (!m_relation->is_valid(m_operation)) {
        throw payload_error{
          fmt::format("{} does not include all mandatory fields",
           m_relation->to_string())
        };
      }
      m_callback.process_relation(*m_relation, m_operation, m_if_unused);
      m_relation.reset(new Relation{});
      m_context.pop_back();
      break;
    case context::in_object:
      m_context.pop_back();
      break;
    }
  }

  void on_enhance_exception(xmlParserInputPtr& location) override {

    try {
        throw;
    } catch (const payload_error& e) {
      throw_with_context(e, location);
    }
  }

private:

  enum class context {
    root,
    top,
    in_create,
    in_modify,
    in_delete,
    node,
    way,
    relation,
    in_object
  };

  template <typename T>
  static void check_attributes(const char **attrs, T check) {
    if (attrs == nullptr)
      return;

    while (*attrs) {
      check(attrs[0], attrs[1]);
      attrs += 2;
    }
  }

  void init_object(OSMObject &object, const char **attrs) {

    check_attributes(attrs, [&object](std::string_view name, const std::string &value) {

      if (name == "id") {
        object.set_id(value);
      } else if (name == "changeset") {
        object.set_changeset(value);
      } else if (name == "version") {
        object.set_version(value);
      } // don't parse any other attributes here
    });

    if (!object.has_id()) {
	throw payload_error{ "Mandatory field id missing in object" };
    }

    if (!object.has_changeset()) {
      throw payload_error{ fmt::format("Changeset id is missing for {}",
                        object.to_string()) };
    }

    if (m_operation == operation::op_create) {
      // we always override version number for create operations (they are not
      // mandatory)
      object.set_version(0u);
    } else if (m_operation == operation::op_delete ||
               m_operation == operation::op_modify) {
      // objects for other operations must have a positive version number
      if (!object.has_version()) {
        throw payload_error{ fmt::format(
                              "Version is required when updating {}",
                          object.to_string()) };
      }
      if (object.version() < 1) {
        throw payload_error{ fmt::format("Invalid version number {} in {}",
                          object.version(), object.to_string()) };
      }
    }
  }

  void init_node(Node &node, const char **attrs) {
    check_attributes(attrs, [&node](std::string_view name, const std::string &value) {

      if (name == "lon") {
        node.set_lon(value);
      } else if (name == "lat") {
        node.set_lat(value);
      }
    });
  }

  void add_tag(OSMObject &o, const char **attrs) {

    std::optional<std::string> k;
    std::optional<std::string> v;

    check_attributes(attrs, [&k, &v](std::string_view name, std::string && value) {

      if (name == "k") {
        k = std::move(value);
      } else if (name == "v") {
        v = std::move(value);
      }
    });

    if (!k)
      throw payload_error{
        fmt::format("Mandatory field k missing in tag element for {}",
         o.to_string())
      };

    if (!v)
      throw payload_error{
        fmt::format("Mandatory field v missing in tag element for {}",
         o.to_string())
      };

    o.add_tag(*k, *v);
  }

  // Include XML message location information where error occurred in exception
  template <typename TEx>
  void throw_with_context(TEx& e, xmlParserInputPtr& location) {

    // Location unknown
    if (location == nullptr)
      throw e;

    throw TEx{ fmt::format("{} at line {:d}, column {:d}",
  	e.what(),
  	location->line,
  	location->col ) };
  }

  operation m_operation = operation::op_undefined;

  std::vector<context> m_context;

  Parser_Callback& m_callback;

  std::unique_ptr<Node> m_node{};
  std::unique_ptr<Way> m_way{};
  std::unique_ptr<Relation> m_relation{};

  bool m_if_unused = false;
};

} // namespace api06

#endif // OSMCHANGE_INPUT_XML_FORMAT_HPP
