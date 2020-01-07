/* exception.h
 *
 * Copyright (C) 2002 The libxml++ development team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __LIBXMLPP_EXCEPTION_H
#define __LIBXMLPP_EXCEPTION_H

#include <exception>
#include <cstdarg> // va_list
#include <string>


extern "C" {
  struct _xmlError;
  struct _xmlParserCtxt;
}

namespace xmlpp
{

/** Base class for all xmlpp exceptions.
 */
class exception : public std::exception
{
public:
  explicit exception(const std::string& message);
  ~exception() noexcept override;

  const char* what() const noexcept override;

  virtual void raise() const;
  virtual exception* clone() const;

private:
  std::string message_;
};

/** Format an _xmlError struct into a text string, suitable for printing.
 *
 * @newin{2,36}
 *
 * @param error Pointer to an _xmlError struct or <tt>nullptr</tt>.
 *              If <tt>nullptr</tt>, the error returned by xmlGetLastError() is used.
 * @returns A formatted text string. If the error struct does not contain an
 *          error (error->code == XML_ERR_OK), an empty string is returned.
 */
std::string format_xml_error(const _xmlError* error = nullptr);

/** Format a parser error into a text string, suitable for printing.
 *
 * @newin{2,36}
 *
 * @param parser_context Pointer to an _xmlParserCtxt struct.
 * @returns A formatted text string. If the parser context does not contain an
 *          error (parser_context->lastError.code == XML_ERR_OK), an empty
 *          string is returned.
 */
std::string format_xml_parser_error(const _xmlParserCtxt* parser_context);

/** Format a message from a function with C-style variadic parameters.
 *
 * Helper function that formats a message supplied in the form of a printf-style
 * format specification and zero or more ... parameters.
 *
 * @code
 * // Typical call:
 * void f(const char* fmt, ...)
 * {
 *   va_list args;
 *   va_start(args, fmt);
 *   std::string msg = xmlpp::format_printf_message(fmt, args);
 *   va_end(args);
 *   // ...
 * }
 * @endcode
 *
 * @newin{3,0}
 */
std::string format_printf_message(const char* fmt, va_list args);

} // namespace xmlpp

#endif // __LIBXMLPP_EXCEPTION_H
