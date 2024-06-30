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

#include "token_parser.h"

#include "dispatcher.h"

namespace SJParser {

void TokenParser::setDispatcher(Dispatcher *dispatcher) noexcept {
  _dispatcher = dispatcher;
}

void TokenParser::reset() {
  _set = false;
  _empty = true;
}

void TokenParser::endParsing() {
  _set = true;
  finish();

  if (_dispatcher != nullptr) {
    _dispatcher->popParser();
  }
}

void TokenParser::on(NullT /*unused*/) {
  reset();

  if (_dispatcher != nullptr) {
    _dispatcher->popParser();
  }
}

void TokenParser::on(bool /*value*/) {
  unexpectedToken("boolean");
}  // LCOV_EXCL_LINE

void TokenParser::on(int64_t /*value*/) {
  unexpectedToken("integer");
}  // LCOV_EXCL_LINE

void TokenParser::on(double /*value*/) {
  unexpectedToken("double");
}  // LCOV_EXCL_LINE

void TokenParser::on(std::string_view /*value*/) {
  unexpectedToken("string");
}  // LCOV_EXCL_LINE

void TokenParser::on(MapStartT /*unused*/) {
  unexpectedToken("map start");
}  // LCOV_EXCL_LINE

void TokenParser::on(MapKeyT /*key*/) {
  unexpectedToken("map key");
}  // LCOV_EXCL_LINE

void TokenParser::on(MapEndT /*unused*/) {
  unexpectedToken("map end");
}  // LCOV_EXCL_LINE

void TokenParser::on(ArrayStartT /*unused*/) {
  unexpectedToken("array start");
}  // LCOV_EXCL_LINE

void TokenParser::on(ArrayEndT /*unused*/) {
  unexpectedToken("array end");
}  // LCOV_EXCL_LINE

void TokenParser::on(DummyT) {
  unexpectedToken("dummy");
}

void TokenParser::childParsed() {}
}  // namespace SJParser
