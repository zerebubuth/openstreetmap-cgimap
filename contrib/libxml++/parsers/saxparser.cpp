/* saxparser.cc
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 *
 * 2002/01/05 Valentin Rusu - fixed some potential buffer overruns
 * 2002/01/21 Valentin Rusu - added CDATA handlers
 */

#include "parsers/saxparser.hpp"

#include <fmt/core.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h> // for xmlCreateFileParserCtxt

#include <cstdarg> //For va_list.

namespace xmlpp {

struct SaxParserCallback
{
  static void start_document(void* context);
  static void end_document(void* context);
  static void start_element(void* context, const xmlChar* name, const xmlChar** p);
  static void end_element(void* context, const xmlChar* name);
  static void characters(void* context, const xmlChar* ch, int len);
  static void warning(void* context, const char* fmt, ...);
  static void error(void* context, const char* fmt, ...);
  static void fatal_error(void* context, const char* fmt, ...);
};



SaxParser::SaxParser()
  : sax_handler_(new _xmlSAXHandler)
{
  xmlSAXHandler temp = {
    nullptr, // internal_subset,
    nullptr, // isStandalone
    nullptr, // hasInternalSubset
    nullptr, // hasExternalSubset
    nullptr, // resolveEntity
    nullptr, // getEntity
    nullptr, // entityDecl
    nullptr, // notationDecl
    nullptr, // attributeDecl
    nullptr, // elementDecl
    nullptr, // unparsedEntityDecl
    nullptr, // setDocumentLocator
    SaxParserCallback::start_document, // startDocument
    SaxParserCallback::end_document, // endDocument
    SaxParserCallback::start_element, // startElement
    SaxParserCallback::end_element, // endElement
    nullptr, // reference
    SaxParserCallback::characters, // characters
    nullptr, // ignorableWhitespace
    nullptr, // processingInstruction
    nullptr, // comment
    SaxParserCallback::warning,  // warning
    SaxParserCallback::error,  // error
    SaxParserCallback::fatal_error, // fatalError
    nullptr, // getParameterEntity
    nullptr, // cdataBlock
    nullptr, // externalSubset
    0,       // initialized
    nullptr, // private
    nullptr, // startElementNs
    nullptr, // endElementNs
    nullptr, // serror
  };
  *sax_handler_ = temp;

  // The default action is to call on_warning(), on_error(), on_fatal_error().
  set_throw_messages(false);
}

SaxParser::~SaxParser()
{
  release_underlying();
}


void SaxParser::on_start_document()
{
}

void SaxParser::on_end_document()
{
}


void SaxParser::on_start_element(const char* name,
                                 const char** p)
{
}

void SaxParser::on_end_element(const char* name)
{
}

void SaxParser::on_characters(const std::string&)
{
}


void SaxParser::on_warning(const std::string& /* text */)
{
}

void SaxParser::on_error(const std::string& /* text */)
{
}


void SaxParser::on_fatal_error(const std::string& text)
{
  throw parse_error("Fatal error: " + text);
}

// implementation of this function is inspired by the SAX documentation by James Henstridge.
// (http://www.daa.com.au/~james/gnome/xml-sax/implementing.html)
void SaxParser::parse()
{
  if(!context_)
  {
    throw internal_error("Parser context not created.");
  }

  auto old_sax = context_->sax;
  context_->sax = sax_handler_.get();

  xmlResetLastError();
  initialize_context();

  const int parseError = xmlParseDocument(context_);

  context_->sax = old_sax;

  auto error_str = format_xml_parser_error(context_);
  if (error_str.empty() && parseError == -1)
    error_str = "xmlParseDocument() failed.";

  release_underlying(); // Free context_

  check_for_exception();

  if(!error_str.empty())
  {
    throw parse_error(error_str);
  }
}

void SaxParser::parse_memory_raw(const unsigned char* contents, size_type bytes_count)
{
  if(context_)
  {
    throw parse_error("Attempt to start a second parse while a parse is in progress.");
  }

  context_ = xmlCreateMemoryParserCtxt((const char*)contents, bytes_count);
  parse();
}

void SaxParser::parse_memory(const std::string& contents)
{
  parse_memory_raw((const unsigned char*)contents.c_str(), contents.size());
}

void SaxParser::release_underlying()
{
  Parser::release_underlying();
}

void SaxParser::initialize_context()
{
  Parser::initialize_context();
}

void SaxParser::on_enhance_exception(xmlParserInputPtr& location)
{
  throw;
}

void SaxParserCallback::start_document(void* context)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  try
  {
    parser->on_start_document();
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

void SaxParserCallback::end_document(void* context)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  if (parser->exception_)
    return;

  try
  {
    parser->on_end_document();
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

void SaxParserCallback::start_element(void* context,
                                        const xmlChar* name,
                                        const xmlChar** p)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  try
  {
    try {
       parser->on_start_element(reinterpret_cast<const char *>(name), reinterpret_cast<const char **>(p));
    } catch (...) {
       parser->on_enhance_exception(the_context->input);
    }
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

void SaxParserCallback::end_element(void* context, const xmlChar* name)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  try
  {
     try {
       parser->on_end_element(reinterpret_cast<const char *>(name));
     } catch(...) {
        parser->on_enhance_exception(the_context->input);
     }
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

void SaxParserCallback::characters(void * context, const xmlChar* ch, int len)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  try
  {
    try {
      parser->on_characters(std::string(reinterpret_cast<const char *>(ch), len) );
    } catch(...) {
      parser->on_enhance_exception(the_context->input);
    }
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

void SaxParserCallback::warning(void* context, const char* fmt, ...)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  va_list arg;
  va_start(arg, fmt);
  const std::string buff = format_printf_message(fmt, arg);
  va_end(arg);

  try
  {
    parser->on_warning(buff);
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

void SaxParserCallback::error(void* context, const char* fmt, ...)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  if (parser->exception_)
    return;

  va_list arg;
  va_start(arg, fmt);
  const std::string buff = format_printf_message(fmt, arg);
  va_end(arg);

  try
  {
    parser->on_error(buff);
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

void SaxParserCallback::fatal_error(void* context, const char* fmt, ...)
{
  auto the_context = static_cast<_xmlParserCtxt*>(context);
  auto parser = static_cast<SaxParser*>(the_context->_private);

  va_list arg;
  va_start(arg, fmt);
  const std::string buff = format_printf_message(fmt, arg);
  va_end(arg);

  try
  {
    parser->on_fatal_error(buff);
  }
  catch (...)
  {
    parser->handle_exception();
  }
}

} // namespace xmlpp
