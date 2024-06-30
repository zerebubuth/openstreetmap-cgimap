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

#include "array_parser.h"

#include "dispatcher.h"

namespace SJParser {

void ArrayParser::reset() {
  TokenParser::reset();

  _parser_ptr->reset();
}

void ArrayParser::on(NullT /*unused*/) {
  if (!_started) {
    TokenParser::on(NullT{});
    return;
  }
  _parser_ptr->on(NullT{});
}

void ArrayParser::on(bool value) {
  if (!_started) {
    unexpectedToken("boolean");
  }
  _parser_ptr->on(value);
  setNotEmpty();
  childParsed();
}

void ArrayParser::on(int64_t value) {
  if (!_started) {
    unexpectedToken("integer");
  }
  _parser_ptr->on(value);
  setNotEmpty();
  childParsed();
}

void ArrayParser::on(double value) {
  if (!_started) {
    unexpectedToken("double");
  }
  _parser_ptr->on(value);
  setNotEmpty();
  childParsed();
}

void ArrayParser::on(std::string_view value) {
  if (!_started) {
    unexpectedToken("string");
  }
  _parser_ptr->on(value);
  setNotEmpty();
  childParsed();
}

void ArrayParser::on(MapStartT /*unused*/) {
  if (!_started) {
    unexpectedToken("map start");
  }
  setNotEmpty();
  _parser_ptr->setDispatcher(dispatcher());
  dispatcher()->pushParser(_parser_ptr);
  _parser_ptr->on(MapStartT{});
}

void ArrayParser::on(ArrayStartT /*unused*/) {
  if (!_started) {
    reset();
    _started = true;
    return;
  }

  setNotEmpty();
  _parser_ptr->setDispatcher(dispatcher());
  dispatcher()->pushParser(_parser_ptr);
  _parser_ptr->on(ArrayStartT{});
}

void ArrayParser::on(ArrayEndT /*unused*/) {
  _started = false;
  endParsing();
}

void ArrayParser::setParserPtr(TokenParser *parser_ptr) {
  _parser_ptr = parser_ptr;
}
}  // namespace SJParser
