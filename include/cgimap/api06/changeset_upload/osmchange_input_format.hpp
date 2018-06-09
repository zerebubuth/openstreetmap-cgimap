#ifndef OSMCHANGE_INPUT_FORMAT_HPP
#define OSMCHANGE_INPUT_FORMAT_HPP

#include "cgimap/api06/changeset_upload/node.hpp"
#include "cgimap/api06/changeset_upload/osmobject.hpp"
#include "cgimap/api06/changeset_upload/parser_callback.hpp"
#include "cgimap/api06/changeset_upload/relation.hpp"
#include "cgimap/api06/changeset_upload/way.hpp"
#include "cgimap/types.hpp"

#include <libxml/parser.h>
#include <boost/format.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

struct xml_error : public http::bad_request {

  std::string error_code;
  std::string error_string;

  explicit xml_error(const std::string &message)
      : http::bad_request(message), error_code(), error_string(message) {}
};

class OSMChangeXMLParser {

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

  context m_context = context::root;
  context m_operation_context = context::root;
  context m_last_context = context::root;

  operation m_operation = operation::op_undefined;

  Parser_Callback *m_callback;

  std::unique_ptr<Node> m_node{};
  std::unique_ptr<Way> m_way{};
  std::unique_ptr<Relation> m_relation{};

  bool m_if_unused = false;

  template <typename T> class XMLParser {

    xmlSAXHandler handler{};
    xmlParserCtxtPtr ctxt{};
    T *m_callback_object;

    static void start_element_wrapper(void *data, const xmlChar *element,
                                      const xmlChar **attrs) {
      static_cast<T *>(data)->start_element((const char *)element,
					    (const char **)attrs);
    }

    static void end_element_wrapper(void *data, const xmlChar *element) {
      static_cast<T *>(data)->end_element((const char *)element);
    }

    static void warning(void *, const char *, ...) {}

    static void error(void *, const char *fmt, ...) {
      char buffer[1024];
      va_list arg_ptr;
      va_start(arg_ptr, fmt);
      vsnprintf(buffer, sizeof(buffer) - 1, fmt, arg_ptr);
      va_end(arg_ptr);
      throw http::bad_request((boost::format("XML Error: %1%") % buffer).str());
    }

    // Don't load any external entities provided in the XML document
    static xmlParserInputPtr xmlCustomExternalEntityLoader(const char *,
                                                           const char *,
                                                           xmlParserCtxtPtr) {
      throw xml_error{ "XML external entities not supported" };
    }

  public:
    explicit XMLParser(T *callback_object)
        : m_callback_object(callback_object) {

      xmlInitParser();

      xmlSetExternalEntityLoader(xmlCustomExternalEntityLoader);

      memset(&handler, 0, sizeof(handler));
      handler.initialized = XML_SAX2_MAGIC;
      handler.startElement = &start_element_wrapper;
      handler.endElement = &end_element_wrapper;
      handler.warning = &warning;
      handler.error = &error;
    }

    XMLParser(const XMLParser &) = delete;
    XMLParser(XMLParser &&) = delete;

    XMLParser &operator=(const XMLParser &) = delete;
    XMLParser &operator=(XMLParser &&) = delete;

    ~XMLParser() noexcept {
      xmlFreeParserCtxt(ctxt);
      xmlCleanupParser();
    }

    void operator()(const std::string &data) {

      const size_t MAX_CHUNKSIZE = 131072;

      if (data.size() < 4)
        throw http::bad_request("Invalid XML input");

      // provide first 4 characters of data string, according to
      // http://xmlsoft.org/library.html
      ctxt = xmlCreatePushParserCtxt(&handler, m_callback_object, data.c_str(),
                                     4, NULL);
      if (ctxt == NULL)
        throw std::runtime_error("Could not create parser context!");

      xmlCtxtUseOptions(ctxt, XML_PARSE_RECOVER | XML_PARSE_NONET);

      unsigned int offset = 4;

      while (offset < data.size()) {

        unsigned int current_chunksize =
            std::min(data.size() - offset, MAX_CHUNKSIZE);

        if (xmlParseChunk(ctxt, data.c_str() + offset, current_chunksize, 0)) {
          xmlErrorPtr err = xmlGetLastError();
          throw std::runtime_error(
              (boost::format("XML ERROR: %1%.") % err->message).str());
        }

        offset += current_chunksize;
      }

      xmlParseChunk(ctxt, 0, 0, 1);
    }

  }; // class XMLParser

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
      } // don't parse any other attributes here
    });

    if (!object.has_id()) {
	throw xml_error{ "Mandatory field id missing in object" };
    }

    if (!object.has_changeset()) {
      throw xml_error{ (boost::format("Changeset id is missing for %1%") %
                        object.to_string())
                           .str() };
    }

    if (m_operation == operation::op_create) {
      // we always override version number for create operations (they are not
      // mandatory)
      object.set_version(0u);
    } else if (m_operation == operation::op_delete ||
               m_operation == operation::op_modify) {
      // objects for other operations must have a positive version number
      if (!object.has_version()) {
        throw xml_error{ (boost::format(
                              "Version is required when updating %1%") %
                          object.to_string())
                             .str() };
      }
      if (object.version() < 1) {
        throw xml_error{ (boost::format("Invalid version number %1% in %2%") %
                          object.version() % object.to_string())
                             .str() };
      }
    }
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

  void add_tag(OSMObject &o, const char **attrs) {

    boost::optional<std::string> k;
    boost::optional<std::string> v;

    check_attributes(attrs, [&k, &v](const char *name, const char *value) {

      if (name[0] == 'k' && name[1] == 0) {
        k = value;
      } else if (name[0] == 'v' && name[1] == 0) {
        v = value;
      }
    });

    if (!k)
      throw xml_error{
        (boost::format("Mandatory field k missing in tag element for %1%") %
         o.to_string())
            .str()
      };

    if (!v)
      throw xml_error{
        (boost::format("Mandatory field v missing in tag element for %1%") %
         o.to_string())
            .str()
      };

    o.add_tag(*k, *v);
  }

  void start_element(const char *element, const char **attrs) {

    switch (m_context) {
    case context::root:
      if (!std::strcmp(element, "osmChange")) {
        m_callback->start_document();
      } else {
        throw xml_error{
          (boost::format("Unknown top-level element %1%, expecting osmChange") %
           element)
              .str()
        };
      }
      m_context = context::top;
      break;

    case context::top:

      if (!std::strcmp(element, "create")) {
        m_context = context::in_create;
        m_operation_context = context::in_create;
        m_operation = operation::op_create;
      } else if (!std::strcmp(element, "modify")) {
        m_context = context::in_modify;
        m_operation_context = context::in_modify;
        m_operation = operation::op_modify;
      } else if (!std::strcmp(element, "delete")) {
        m_if_unused = false;
        check_attributes(attrs, [&](const char *name, const char *) {

          if (!std::strcmp(name, "if-unused")) {
            m_if_unused = true;
          }
        });
        m_context = context::in_delete;
        m_operation_context = context::in_delete;
        m_operation = operation::op_delete;
      } else {
        throw xml_error{
          (boost::format(
               "Unknown action %1%, choices are create, modify, delete") %
           element)
              .str()
        };
      }
      break;

    case context::in_delete:
    case context::in_create:
    case context::in_modify:

      if (!std::strcmp(element, "node")) {
        m_node.reset(new Node{});
        init_object(*m_node, attrs);
        init_node(*m_node, attrs);
        m_context = context::node;
      } else if (!std::strcmp(element, "way")) {
        m_way.reset(new Way{});
        init_object(*m_way, attrs);
        m_context = context::way;
      } else if (!std::strcmp(element, "relation")) {
        m_relation.reset(new Relation{});
        init_object(*m_relation, attrs);
        m_context = context::relation;
      } else {
        throw xml_error{
          (boost::format(
               "Unknown element %1%, expecting node, way or relation") %
           element)
              .str()
        };
      }
      break;

    case context::node:
      m_last_context = context::node;
      m_context = context::in_object;
      if (!std::strcmp(element, "tag")) {
        add_tag(*m_node, attrs);
      }
      break;
    case context::way:
      m_last_context = context::way;
      m_context = context::in_object;
      if (!std::strcmp(element, "nd")) {
	bool ref_found = false;
        check_attributes(attrs, [&](const char *name, const char *value) {
          if (!std::strcmp(name, "ref")) {
            m_way->add_way_node(value);
            ref_found = true;
          }
        });
        if (!ref_found)
          throw xml_error{ (boost::format(
                                "Missing mandatory ref field on way node %1%") %
                            m_way->to_string())
                               .str() };
      } else if (!std::strcmp(element, "tag")) {
        add_tag(*m_way, attrs);
      }
      break;
    case context::relation:
      m_last_context = context::relation;
      m_context = context::in_object;
      if (!std::strcmp(element, "member")) {
        RelationMember member;
        check_attributes(attrs, [&member](const char *name, const char *value) {
          if (!std::strcmp(name, "type")) {
            member.set_type(value);
          } else if (!std::strcmp(name, "ref")) {
            member.set_ref(value);
          } else if (!std::strcmp(name, "role")) {
            member.set_role(value);
          }
        });
        if (!member.is_valid()) {
          throw xml_error{ (boost::format(
                                "Missing mandatory field on relation member in %1%") %
                            m_relation->to_string())
                               .str() };
        }
        m_relation->add_member(member);
      } else if (!std::strcmp(element, "tag")) {
        add_tag(*m_relation, attrs);
      }
      break;
    case context::in_object:
      throw xml_error{ "xml file nested too deep" };
      break;
    }
  }

  void end_element(const char *element) {

    switch (m_context) {
    case context::root:
      assert(false);
      break;
    case context::top:
      assert(!std::strcmp(element, "osmChange"));
      m_context = context::root;
      m_operation = operation::op_undefined;
      m_callback->end_document();
      break;
    case context::in_create:
      assert(!std::strcmp(element, "create"));
      m_context = context::top;
      m_operation_context = context::top;
      m_operation = operation::op_undefined;
      break;
    case context::in_modify:
      assert(!std::strcmp(element, "modify"));
      m_context = context::top;
      m_operation_context = context::top;
      m_operation = operation::op_undefined;
      break;
    case context::in_delete:
      assert(!std::strcmp(element, "delete"));
      m_context = context::top;
      m_operation_context = context::top;
      m_operation = operation::op_undefined;
      m_if_unused = false;
      break;
    case context::node:
      assert(!std::strcmp(element, "node"));
      if (!m_node->is_valid(m_operation)) {
        throw xml_error{
          (boost::format("Object %1% does not include all mandatory fields") %
           m_node->to_string())
              .str()
        };
      }
      m_callback->process_node(*m_node, m_operation, m_if_unused);
      m_node.reset(new Node{});
      m_context = m_operation_context;
      break;
    case context::way:
      assert(!std::strcmp(element, "way"));
      if (!m_way->is_valid(m_operation)) {
        throw xml_error{
          (boost::format("Object %1% does not include all mandatory fields") %
           m_way->to_string())
              .str()
        };
      }

      m_callback->process_way(*m_way, m_operation, m_if_unused);
      m_way.reset(new Way{});
      m_context = m_operation_context;
      break;
    case context::relation:
      assert(!std::strcmp(element, "relation"));
      if (!m_relation->is_valid()) {
        throw xml_error{
          (boost::format("Object %1% does not include all mandatory fields") %
           m_relation->to_string())
              .str()
        };
      }
      m_callback->process_relation(*m_relation, m_operation, m_if_unused);
      m_relation.reset(new Relation{});
      m_context = m_operation_context;
      break;
    case context::in_object:
      m_context = m_last_context;
      break;
    }
  }

public:
  explicit OSMChangeXMLParser(Parser_Callback *callback)
      : m_callback(callback) {}

  OSMChangeXMLParser(const OSMChangeXMLParser &) = delete;
  OSMChangeXMLParser &operator=(const OSMChangeXMLParser &) = delete;

  OSMChangeXMLParser(OSMChangeXMLParser &&) = delete;
  OSMChangeXMLParser &operator=(OSMChangeXMLParser &&) = delete;

  void process_message(const std::string &data) {

    XMLParser<OSMChangeXMLParser> parser{ this };

    parser(data);
  }
};

#endif // OSMCHANGE_INPUT_FORMAT_HPP
