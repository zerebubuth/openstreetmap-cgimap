/* saxparser.h
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 */

#ifndef __LIBXMLPP_PARSERS_SAXPARSER_H
#define __LIBXMLPP_PARSERS_SAXPARSER_H

#include <memory>

#include <libxml/parser.h>
#include "parse_error.hpp"
#include "parser.hpp"
#include "wrapped_exception.hpp"

extern "C" {
  struct _xmlSAXHandler;
  struct _xmlEntity;
}


namespace xmlpp {

/** SAX XML parser.
 * Derive your own class and override the on_*() methods.
 * SAX = Simple API for XML
 */
class SaxParser : public Parser
{
public:

  SaxParser();
  ~SaxParser() override;

  /** Parse an XML document from a string.
   * @param contents The XML document as a string.
   * @throws xmlpp::internal_error
   * @throws xmlpp::parse_error
   * @throws xmlpp::validity_error
   */
  void parse_memory(const std::string& contents) override;

  /** Parse an XML document from raw memory.
   * @param contents The XML document as an array of bytes.
   * @param bytes_count The number of bytes in the @a contents array.
   * @throws xmlpp::internal_error
   * @throws xmlpp::parse_error
   * @throws xmlpp::validity_error
   */
  void parse_memory_raw(const unsigned char* contents, size_type bytes_count) override;

protected:
  virtual void on_start_document();
  virtual void on_end_document();
  virtual void on_start_element(const char* name, const char** p);
  virtual void on_end_element(const char* name);
  virtual void on_warning(const std::string& text);
  virtual void on_error(const std::string& text);
  virtual void on_characters(const std::string& characters);

  // provides current location information to parser to provide option
  // to enhance exception with location information
  virtual void on_enhance_exception(xmlParserInputPtr& location);

  /** @throws xmlpp::parse_error
   */
  virtual void on_fatal_error(const std::string& text);

  void release_underlying() override;
  void initialize_context() override;

private:
  void parse();

  std::unique_ptr<_xmlSAXHandler> sax_handler_;

  friend struct SaxParserCallback;
};

} // namespace xmlpp

#endif //__LIBXMLPP_PARSERS_SAXPARSER_H
