#ifndef NODE_INPUT_FORMAT_HPP
#define NODE_INPUT_FORMAT_HPP

#include "cgimap/api06/changeset_upload/osmobject_input_format.hpp"

namespace api06 {

class NodeXMLParser : private OSMObjectXMLParser {

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
          add_tag(*m_node, attrs);
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

private:

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
