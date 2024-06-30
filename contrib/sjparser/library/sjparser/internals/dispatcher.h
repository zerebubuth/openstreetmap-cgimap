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

#pragma once

#include <deque>
#include <functional>
#include <string>

#include "token_parser.h"

namespace SJParser {

class Dispatcher {
 public:
  explicit Dispatcher(TokenParser *parser);
  void pushParser(TokenParser *parser);
  void popParser();
  [[nodiscard]] bool emptyParsersStack() const;
  void reset();

  template <typename TokenT> void on(TokenT value);

 private:
  std::deque<TokenParser *> _parsers;
  TokenParser *_root_parser = nullptr;
};

/****************************** Implementations *******************************/

template <typename TokenT> void Dispatcher::on(TokenT value) {
  if (_parsers.empty()) {
    throw std::runtime_error("Parsers stack is empty");
  }
  _parsers.back()->on(value);
}

}  // namespace SJParser
