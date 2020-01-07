/* parser.h
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 */

#ifndef __LIBXMLPP_PARSER_H
#define __LIBXMLPP_PARSER_H

#include <string>
#include <istream>
#include <cstdarg> // va_list
#include <memory> // std::unique_ptr
#include "exception.hpp"
#include "internal_error.hpp"


extern "C" {
  struct _xmlParserCtxt;
}

namespace xmlpp {

/** XML parser.
 *
 * Abstract base class for DOM parser and SAX parser.
 */
class Parser
{
public:
  Parser();
  virtual ~Parser();

  Parser( const Parser& ) = delete;
  Parser& operator=( const Parser& ) = delete;

  using size_type = unsigned int;


  /** Set whether the parser will collect and throw error and warning messages.
   *
   * If messages are collected, they are included in an exception thrown at the
   * end of parsing.
   *
   * - SAX parser
   *
   *   If the messages are not collected, the error handling on_*() methods in
   *   the user's SAX parser subclass are called. This is the default, if
   *   set_throw_messages() is not called.
   *
   * @newin{2,36}
   *
   * @param val Whether messages will be collected and thrown in an exception.
   */
  void set_throw_messages(bool val = true) noexcept;

  /** See set_throw_messages().
   *
   * @newin{2,36}
   *
   * @returns Whether messages will be collected and thrown in an exception.
   */
  bool get_throw_messages() const noexcept;


  /** Set and/or clear parser option flags.
   * See the libxml2 documentation, enum xmlParserOption, for a list of parser options.
   * This method overrides other methods that set parser options, such as set_validate(),
   * set_substitute_entities() and set_include_default_attributes(). Use set_parser_options()
   * only if no other method can set the parser options you want.
   *
   * @newin{2,38}
   *
   * @param set_options Set bits correspond to flags that shall be set during parsing.
   * @param clear_options Set bits correspond to flags that shall be cleared during parsing.
   *        Bits that are set in neither @a set_options nor @a clear_options are not affected.
   */
  void set_parser_options(int set_options = 0, int clear_options = 0) noexcept;

  /** See set_parser_options().
   *
   * @newin{2,38}
   *
   * @param[out] set_options Set bits correspond to flags that shall be set during parsing.
   * @param[out] clear_options Set bits correspond to flags that shall be cleared during parsing.
   *        Bits that are set in neither @a set_options nor @a clear_options are not affected.
   */
  void get_parser_options(int& set_options, int& clear_options) const noexcept;

  /** Parse an XML document from a file.
   * @throw exception
   * @param filename The path to the file.
   */
  virtual void parse_file(const std::string& filename) = 0;

  /** Parse an XML document from raw memory.
   * @throw exception
   * @param contents The XML document as an array of bytes.
   * @param bytes_count The number of bytes in the @a contents array.
   */
  virtual void parse_memory_raw(const unsigned char* contents, size_type bytes_count) = 0;

  /** Parse an XML document from a string.
   * @throw exception
   * @param contents The XML document as a string.
   */
  virtual void parse_memory(const std::string& contents) = 0;

  /** Parse an XML document from a stream.
   * @throw exception
   * @param in The stream.
   */
  virtual void parse_stream(std::istream& in) = 0;

protected:
  virtual void initialize_context();
  virtual void release_underlying();

  virtual void on_generic_error(const std::string& message);
  virtual void on_parser_error(const std::string& message);
  virtual void on_parser_warning(const std::string& message);

  /// To be called in an exception handler.
  virtual void handle_exception();
  virtual void check_for_exception();

  virtual void check_for_error_and_warning_messages();

  static void callback_parser_error(void* ctx, const char* msg, ...);
  static void callback_parser_warning(void* ctx, const char* msg, ...);

  static void callback_generic_error(void* ctx, const char* msg, ...);

  enum class MsgType
  {
    GenericError,
    ParserError,
    ParserWarning
  };

  static void callback_error_or_warning(MsgType msg_type, void* ctx,
                                        const char* msg, va_list var_args);

  _xmlParserCtxt* context_;
  std::unique_ptr<exception> exception_;

private:
  struct Impl;
  std::unique_ptr<Impl> pimpl_;
};

/** Equivalent to Parser::parse_stream().
 *
 * @newin{2,38}
 */
inline std::istream& operator>>(std::istream& in, Parser& parser)
{
  parser.parse_stream(in);
  return in;
}

} // namespace xmlpp

#endif //__LIBXMLPP_PARSER_H

