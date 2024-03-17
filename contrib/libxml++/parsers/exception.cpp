#include "parsers/exception.hpp"

#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <cstdio>
#include <vector>

namespace xmlpp {

exception::exception(const std::string& message)
: message_(message)
{
}

exception::~exception() noexcept
{}

const char* exception::what() const noexcept
{
  return message_.c_str();
}

void exception::raise() const
{
  throw *this;
}

exception* exception::clone() const
{
  return new exception(*this);
}

std::string format_xml_error(const _xmlError* error)
{
  if (!error)
    error = xmlGetLastError();

  if (!error || error->code == XML_ERR_OK)
    return ""; // No error

  std::string str;

  if (error->file && *error->file != '\0')
  {
    str += "File ";
    str += error->file;
  }

  if (error->line > 0)
  {
    str += (str.empty() ? "Line " : ", line ") + std::to_string(error->line);
    if (error->int2 > 0)
      str += ", column " + std::to_string(error->int2);
  }

  const bool two_lines = !str.empty();
  if (two_lines)
    str += ' ';

  switch (error->level)
  {
    case XML_ERR_WARNING:
      str += "(warning):";
      break;
    case XML_ERR_ERROR:
      str += "(error):";
      break;
    case XML_ERR_FATAL:
      str += "(fatal):";
      break;
    default:
      str += "():";
      break;
  }

  str += two_lines ? '\n' : ' ';

  if (error->message && *error->message != '\0')
    str += error->message;
  else
    str += "Error code " + std::to_string(error->code);

  // If the string does not end with end-of-line, append an end-of-line.
  if (*str.rbegin() != '\n')
    str += '\n';

  return str;
}

std::string format_xml_parser_error(const _xmlParserCtxt* parser_context)
{
  if (!parser_context)
    return "Error. xmlpp::format_xml_parser_error() called with parser_context == nullptr\n";

  const auto error = xmlCtxtGetLastError(const_cast<_xmlParserCtxt*>(parser_context));

  if (!error)
    return ""; // No error

  std::string str;

  if (!parser_context->wellFormed)
    str += "Document not well-formed.\n";

  return str + format_xml_error(error);
}

std::string format_printf_message(const char* fmt, va_list args)
{
  // This code was inspired by the example at
  // http://en.cppreference.com/w/cpp/io/c/vfprintf
  va_list args2;
  va_copy(args2, args);
  // Number of characters (bytes) in the resulting string;
  // error, if < 0.
  const int nchar = std::vsnprintf(nullptr, 0, fmt, args2);
  va_end(args2);
  if (nchar < 0)
   return std::string("Error code from std::vsnprintf = " + std::to_string(nchar));

  std::vector<char> buf(nchar+1);
  std::vsnprintf(buf.data(), buf.size(), fmt, args);
  return std::string(buf.data());
}

} //namespace xmlpp
