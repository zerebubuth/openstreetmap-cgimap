/*******************************************************************************

Copyright (c) 2016-2017 Denis Tikhomirov <dvtikhomirov@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "parsing_error.h"

namespace SJParser {

ParsingError::ParsingError(std::string sjparser_error, std::string parser_error)
    : _sjparser_error{std::move(sjparser_error)},
      _parser_error{std::move(parser_error)} {}

const char *ParsingError::what() const noexcept {
  if (!_sjparser_error.empty()) {
    return _sjparser_error.c_str();
  }
  return _parser_error.c_str();
}

const std::string &ParsingError::sjparserError() const noexcept {
  return _sjparser_error;
}

const std::string &ParsingError::parserError() const noexcept {
  return _parser_error;
}
}  // namespace SJParser
