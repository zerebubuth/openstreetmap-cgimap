/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef CHANGESET_INPUT_FORMAT_HPP
#define CHANGESET_INPUT_FORMAT_HPP

#include "cgimap/util.hpp"
#include "cgimap/api06/changeset_upload/osmobject.hpp"

#include <libxml/parser.h>
#include <fmt/core.h>

#include <cassert>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "parsers/saxparser.hpp"

namespace api06 {

  class ChangesetXMLParser : private xmlpp::SaxParser {

  public:
    explicit ChangesetXMLParser() = default;

    ~ChangesetXMLParser() override = default;

    ChangesetXMLParser(const ChangesetXMLParser &) = delete;
    ChangesetXMLParser &operator=(const ChangesetXMLParser &) = delete;

    ChangesetXMLParser(ChangesetXMLParser &&) = delete;
    ChangesetXMLParser &operator=(ChangesetXMLParser &&) = delete;

    std::map<std::string, std::string> process_message(const std::string &data) {

      try {
        parse_memory(data);
      } catch (const xmlpp::exception& e) {
        throw http::bad_request(e.what());    // rethrow XML parser error as HTTP 400 Bad request
      }
      return tags();
    }

  protected:

    void on_start_element(const char *elem, const char **attrs) override {

      const std::string_view element(elem);

      switch (m_context) {
	case context::root:
	  if (element == "osm")
	    m_context = context::top;
	  else
	    throw payload_error{ "Unknown top-level element, expecting osm"  };

	  break;

	case context::top:
	  if (element == "changeset") {
	    m_context = context::in_changeset;
	    changeset_element_found = true;
	  }
	  else
	    throw payload_error{ "Unknown element, expecting changeset" };
	  break;

	case context::in_changeset:
	  if (element == "tag") {
	    m_context = context::in_tag;
	    add_tag(attrs);
	  }
	  else
	    throw payload_error{ "Unknown element, expecting tag" };
	  break;

	case context::in_tag:
	  // intentionally not implemented
	  break;
      }
    }

    void on_end_element(const char *elem) override {

      const std::string_view element(elem);

      switch (m_context) {
	case context::root:
	  assert(false);
	  break;
	case context::top:
	  assert(element == "osm");
	  m_context = context::root;
	  if (!changeset_element_found)
	    throw payload_error{ "Cannot parse valid changeset from xml string. XML doesn't contain an osm/changeset element" };
	  break;
	case context::in_changeset:
	  assert(element == "changeset");
	  m_context = context::top;
	  break;
	case context::in_tag:
	  assert(element == "tag");
	  m_context = context::in_changeset;
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

    std::map<std::string, std::string> tags() const { return m_tags; }

    void add_tag(const std::string &key, const std::string &value) {

      if (key.empty())
	throw payload_error("Key may not be empty");

      if (unicode_strlen(key) > 255)
	throw payload_error("Key has more than 255 unicode characters");

      if (unicode_strlen(value) > 255)
	throw payload_error("Value has more than 255 unicode characters");

      m_tags[key] = value;

    }

    template <typename T>
    static void check_attributes(const char **attrs, T check) {
      if (attrs == nullptr)
	return;

      while (*attrs) {
	  check(attrs[0], attrs[1]);
	  attrs += 2;
      }
    }

    void add_tag(const char **attrs) {

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
	throw payload_error{"Mandatory field k missing in tag element"};

      if (!v)
	throw payload_error{"Mandatory field v missing in tag element"};

      add_tag(*k, *v);
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

    enum class context {
      root,
      top,
      in_changeset,
      in_tag
    };

    context m_context = context::root;

    std::map<std::string, std::string> m_tags;
    bool changeset_element_found = false;

  };

} // namespace api06

#endif // CHANGESET_INPUT_FORMAT_HPP
