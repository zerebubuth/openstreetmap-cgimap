#ifndef CHANGESET_INPUT_FORMAT_HPP
#define CHANGESET_INPUT_FORMAT_HPP

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <libxml/parser.h>
#include <fmt/core.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include "parsers/saxparser.hpp"

namespace api06 {



  class ChangesetXMLParser : private xmlpp::SaxParser {

  public:
    explicit ChangesetXMLParser() {}

    virtual ~ChangesetXMLParser()  { }

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

    void on_start_element(const char *element, const char **attrs) override {

      switch (m_context) {
	case context::root:
	  if (!std::strcmp(element, "osm"))
	    m_context = context::top;
	  else
	    throw xml_error{ "Unknown top-level element, expecting osm"  };

	  break;

	case context::top:
	  if (!std::strcmp(element, "changeset")) {
	    m_context = context::in_changeset;
	    changeset_element_found = true;
	  }
	  else
	    throw xml_error{ "Unknown element, expecting changeset" };
	  break;

	case context::in_changeset:
	  if (!std::strcmp(element, "tag")) {
	    m_context = context::in_tag;
	    add_tag(attrs);
	  }
	  else
	    throw xml_error{ "Unknown element, expecting tag" };
	  break;

	case context::in_tag:
	  // intentionally not implemented
	  break;
      }
    }

    void on_end_element(const char *element) override {

      switch (m_context) {
	case context::root:
	  assert(false);
	  break;
	case context::top:
	  assert(!std::strcmp(element, "osm"));
	  m_context = context::root;
	  if (!changeset_element_found)
	    throw xml_error{ "Cannot parse valid changeset from xml string. XML doesn't contain an osm/changeset element" };
	  break;
	case context::in_changeset:
	  assert(!std::strcmp(element, "changeset"));
	  m_context = context::top;
	  break;
	case context::in_tag:
	  assert(!std::strcmp(element, "tag"));
	  m_context = context::in_changeset;
      }
    }

    void on_enhance_exception(xmlParserInputPtr& location) override {

      try {
          throw;
      } catch (const xml_error& e) {
        throw_with_context(e, location);
      }
    }

  private:

    std::map<std::string, std::string> tags() const { return m_tags; }

    void add_tag(std::string key, std::string value) {

      if (key.empty())
	throw xml_error("Key may not be empty");

      if (unicode_strlen(key) > 255)
	throw xml_error("Key has more than 255 unicode characters");

      if (unicode_strlen(value) > 255)
	throw xml_error("Value has more than 255 unicode characters");

      m_tags[key] = value;

    }

    template <typename T>
    static void check_attributes(const char **attrs, T check) {
      if (attrs == NULL)
	return;

      while (*attrs) {
	  check(attrs[0], attrs[1]);
	  attrs += 2;
      }
    }

    void add_tag(const char **attrs) {

      std::optional<std::string> k;
      std::optional<std::string> v;

      check_attributes(attrs, [&k, &v](const char *name, const char *value) {

	if (name[0] == 'k' && name[1] == 0) {
	    k = value;
	} else if (name[0] == 'v' && name[1] == 0) {
	    v = value;
	}
      });

      if (!k)
	throw xml_error{"Mandatory field k missing in tag element"};

      if (!v)
	throw xml_error{"Mandatory field v missing in tag element"};

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
