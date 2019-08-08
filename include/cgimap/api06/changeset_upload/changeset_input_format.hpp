#ifndef CHANGESET_INPUT_FORMAT_HPP
#define CHANGESET_INPUT_FORMAT_HPP

#include "cgimap/types.hpp"
#include "cgimap/util.hpp"

#include <libxml/parser.h>
#include <boost/format.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace api06 {



  class ChangesetXMLParser {

  public:
    explicit ChangesetXMLParser() {}

    ChangesetXMLParser(const ChangesetXMLParser &) = delete;
    ChangesetXMLParser &operator=(const ChangesetXMLParser &) = delete;

    ChangesetXMLParser(ChangesetXMLParser &&) = delete;
    ChangesetXMLParser &operator=(ChangesetXMLParser &&) = delete;

    std::map<std::string, std::string> process_message(const std::string &data) {

      XMLParser<ChangesetXMLParser> parser{ this };

      parser(data);

      return tags();
    }

  private:

    std::map<std::string, std::string> tags() const { return m_tags; }

    void add_tag(std::string key, std::string value) {

      if (key.empty())
	throw xml_error("Key may not be empty in %1%");

      if (unicode_strlen(key) > 255)
	throw xml_error("Key has more than 255 unicode characters");

      if (unicode_strlen(value) > 255)
	throw xml_error("Value has more than 255 unicode characters");

      m_tags[key] = value;

    }

    template <typename T> class XMLParser {

      xmlSAXHandler handler{};
      xmlParserCtxtPtr ctxt{};
      T *m_callback_object;

      struct user_data_t {
	T *callback;
	std::function<std::pair<int, int>(void)> current_location;
      };

      // Include XML message location information where error occurred in exception
      template <typename TEx>
      static void throw_with_context(void *data, TEx& e) {
	auto location = static_cast<user_data_t *>(data)->current_location();

	// Location unknown
	if (location.first == 0 && location.second == 0)
	  throw e;

	throw TEx{ (boost::format("%1% at line %2%, column %3%") %
	    e.what() %
	    location.first %
	    location.second )
	  .str() };
      }

      static void start_element_wrapper(void *data, const xmlChar *element,
					const xmlChar **attrs) {
	try {
	    static_cast<user_data_t *>(data)->callback->
		start_element(reinterpret_cast<const char *>(element),
			      reinterpret_cast<const char **>(attrs));
	} catch (xml_error& e) {
	    throw_with_context(data, e);
	}
      }

      static void end_element_wrapper(void *data, const xmlChar *element) {
	try {
	    static_cast<user_data_t *>(data)->callback->
		end_element(reinterpret_cast<const char *>(element));
	} catch (xml_error& e) {
	    throw_with_context(data, e);
	}
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

	user_data_t user_data;
	user_data.callback = m_callback_object;
	user_data.current_location = std::bind(&XMLParser::get_current_location, this);

	// provide first 4 characters of data string, according to
	// http://xmlsoft.org/library.html
	ctxt = xmlCreatePushParserCtxt(&handler, &user_data, data.c_str(),
				       4, NULL);
	if (ctxt == NULL)
	  throw std::runtime_error("Could not create parser context!");

	xmlCtxtUseOptions(ctxt, XML_PARSE_NONET | XML_PARSE_NOENT);

	unsigned int offset = 4;

	while (offset < data.size()) {

	    unsigned int current_chunksize =
		std::min(data.size() - offset, MAX_CHUNKSIZE);

	    if (xmlParseChunk(ctxt, data.c_str() + offset, current_chunksize, 0)) {
		xmlErrorPtr err = xmlGetLastError();
		throw http::bad_request(
		    (boost::format("XML ERROR: %1%.") % err->message).str());
	    }
	    offset += current_chunksize;
	}

	xmlParseChunk(ctxt, 0, 0, 1);
      }

    private:

      std::pair<int, int> get_current_location() {
	if (ctxt->input == nullptr)
	  return {};
	return { ctxt->input->line, ctxt->input->col };
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

    void add_tag(const char **attrs) {

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
	throw xml_error{"Mandatory field k missing in tag element"};

      if (!v)
	throw xml_error{"Mandatory field v missing in tag element"};

      add_tag(*k, *v);
    }

    void start_element(const char *element, const char **attrs) {

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

      }
    }

    void end_element(const char *element) {

      switch (m_context) {
	case context::root:
	  assert(false);
	  break;
	case context::top:
	  assert(!std::strcmp(element, "osm"));
	  m_context = context::root;
	  if (!changeset_element_found)
	    throw xml_error{ "Cannot parse valid changeset from xml string. XML doesn't contain an osm/changeset element." };
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
