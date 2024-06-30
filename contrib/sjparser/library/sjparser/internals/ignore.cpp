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

#include "ignore.h"

namespace SJParser {

void Ignore::reset() {
  TokenParser::reset();

  _structure.clear();
}

void Ignore::onValue() {
  if (_structure.empty()) {
    endParsing();
  }
}

void Ignore::on(bool /*value*/) {
  onValue();
}

void Ignore::on(int64_t /*value*/) {
  onValue();
}

void Ignore::on(double /*value*/) {
  onValue();
}

void Ignore::on(std::string_view /*value*/) {
  onValue();
}

void Ignore::on(MapStartT /*unused*/) {
  _structure.push_back(Structure::Object);
}

void Ignore::on(MapKeyT /*key*/) {
  if (_structure.empty() || (_structure.back() != Structure::Object)) {
    unexpectedToken("map key");
  }
}

void Ignore::on(MapEndT /*unused*/) {
  if (_structure.empty() || (_structure.back() != Structure::Object)) {
    unexpectedToken("map end");
  }
  _structure.pop_back();

  if (_structure.empty()) {
    endParsing();
  }
}

void Ignore::on(ArrayStartT /*unused*/) {
  _structure.push_back(Structure::Array);
}

void Ignore::on(ArrayEndT /*unused*/) {
  if (_structure.empty() || (_structure.back() != Structure::Array)) {
    unexpectedToken("array end");
  }
  _structure.pop_back();

  if (_structure.empty()) {
    endParsing();
  }
}

void Ignore::finish() {}
}  // namespace SJParser
