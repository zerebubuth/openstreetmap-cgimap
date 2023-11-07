#ifndef NODE_INPUT_FORMAT_HPP
#define NODE_INPUT_FORMAT_HPP

#include "cgimap/api06/changeset_upload/node.hpp"
#include "parsers/saxparser.hpp"

namespace api06 {

class NodeXMLParser : private xmlpp::SaxParser {

public:

  std::unique_ptr<Node> process_message(const std::string &data) {

    try {
      parse_memory(data);
    } catch (const xmlpp::exception& e) {
      throw http::bad_request(e.what());    // rethrow XML parser error as HTTP 400 Bad request
    }
    assert(m_node);

    return std::move(m_node);
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
        if (!std::strcmp(element, "node")) {
          m_context = context::in_object;
          m_node.reset(new Node{});
          init_object(*m_node, attrs);
          init_node(*m_node, attrs);
        }
        else
          throw xml_error{ "Unknown element, expecting node" };
        break;

      case context::in_object:
        if (!std::strcmp(element, "tag")) {
          m_context = context::in_tag;
          // add_tag(attrs); // TODO
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
        if (!m_node)
          throw xml_error{ "Cannot parse valid node from xml string. XML doesn't contain an osm/node element" };
        break;

      case context::in_object:
        assert(!std::strcmp(element, "node"));
        m_context = context::top;
        break;

      case context::in_tag:
        assert(!std::strcmp(element, "tag"));
        m_context = context::in_object;
        break;
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

  template <typename T>
  static void check_attributes(const char **attrs, T check) {

    if (attrs == NULL)
      return;

    while (*attrs) {
      check(attrs[0], attrs[1]);
      attrs += 2;
    }
  }
  
  void init_object(OSMObject &object, const char **attrs) {

    check_attributes(attrs, [&object](const char *name, const char *value) {
      if (!std::strcmp(name, "id")) {
        object.set_id(value);
      } else if (!std::strcmp(name, "changeset")) {
        object.set_changeset(value);
      } else if (!std::strcmp(name, "version")) {
        object.set_version(value);
      }
    });
  }

  void init_node(Node &node, const char **attrs) {

    check_attributes(attrs, [&node](const char *name, const char *value) {
      if (!std::strcmp(name, "lon")) {
        node.set_lon(value);
      } else if (!std::strcmp(name, "lat")) {
        node.set_lat(value);
      }
    });
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
    in_object,
    in_tag
  };
  
  context m_context = context::root;

  std::unique_ptr<Node> m_node;

};

} // namespace api06

#endif // NODE_INPUT_FORMAT_HPP
