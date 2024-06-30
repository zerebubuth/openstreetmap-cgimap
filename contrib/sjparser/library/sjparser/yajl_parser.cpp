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

#include "yajl_parser.h"

#include "parsing_error.h"

namespace SJParser {

YajlParser::YajlParser() : _yajl_handle{nullptr, yajl_free} {
  resetYajlHandle();
}

void YajlParser::setTokenParser(TokenParser *parser) {
  _dispatcher = std::make_unique<Dispatcher>(parser);
}

void YajlParser::resetYajlHandle() {
  _yajl_handle.reset(yajl_alloc(&_parser_yajl_callbacks, nullptr, this));
  if (_yajl_handle == nullptr) {
    throw std::runtime_error("Can not allocate YAJL handle");  // LCOV_EXCL_LINE
  }
}

void YajlParser::parse(const std::string &data) {
  parse(data.data(), data.size());
}

void YajlParser::parse(const char *data, size_t len) {
  _data = reinterpret_cast<const unsigned char *>(data);
  _len = len;

  if (_reset_needed) {
    _dispatcher->reset();
    resetYajlHandle();
    _sjparser_error = "";

    _reset_needed = false;
  }

  if (yajl_parse(_yajl_handle.get(), _data, _len) != yajl_status_ok) {
    _reset_needed = true;
    throwParsingError();
  }
}

void YajlParser::finish() {
  _reset_needed = true;

  if (yajl_complete_parse(_yajl_handle.get()) != yajl_status_ok) {
    throwParsingError();
  } else {
    checkDispatcherStack();
  }
}

void YajlParser::checkDispatcherStack() const {
  if (_dispatcher->emptyParsersStack()) {
    return;
  }

  throw ParsingError("Dispatcher parsers stack is not empty in the end");
}

[[noreturn]] void YajlParser::throwParsingError() {
  std::string yajl_error;

  if (auto *yajl_error_ptr = yajl_get_error(_yajl_handle.get(), 1, _data, _len);
      yajl_error_ptr) {
    yajl_error = reinterpret_cast<char *>(yajl_error_ptr);
    yajl_free_error(_yajl_handle.get(), yajl_error_ptr);
  } else {
    yajl_error = "Unknown YAJL error\n";  // LCOV_EXCL_LINE
  }
  std::string error_offset =
      "\nError occurred at offset "
      + std::to_string(yajl_get_bytes_consumed(_yajl_handle.get())) + ".";
  if (!_sjparser_error.empty()) {
    _sjparser_error += error_offset;
  }
  if (!yajl_error.empty()) {
    yajl_error += error_offset;
  }
  throw ParsingError(_sjparser_error, yajl_error);
}

int YajlParser::yajlOnNull(void *ctx) {
  return static_cast<YajlParser *>(ctx)->on(NullT{});
}

int YajlParser::yajlOnBool(void *ctx, int value) {
  return static_cast<YajlParser *>(ctx)->on(static_cast<bool>(value));
}

// Disable google-runtime-int long long -> int64_t
int YajlParser::yajlOnInt(void *ctx, long long value) {  // NOLINT
  return static_cast<YajlParser *>(ctx)->on(static_cast<int64_t>(value));
}

int YajlParser::yajlOnDouble(void *ctx, double value) {
  return static_cast<YajlParser *>(ctx)->on(value);
}

int YajlParser::yajlOnString(void *ctx, const unsigned char *value,
                             size_t len) {
  return static_cast<YajlParser *>(ctx)->on(
      std::string_view(reinterpret_cast<const char *>(value), len));
}

int YajlParser::yajlOnMapStart(void *ctx) {
  return static_cast<YajlParser *>(ctx)->on(MapStartT{});
}

int YajlParser::yajlOnMapKey(void *ctx, const unsigned char *value,
                             size_t len) {
  return static_cast<YajlParser *>(ctx)->on(
      MapKeyT{std::string_view(reinterpret_cast<const char *>(value), len)});
}

int YajlParser::yajlOnMapEnd(void *ctx) {
  return static_cast<YajlParser *>(ctx)->on(MapEndT{});
}

int YajlParser::yajlOnArrayStart(void *ctx) {
  return static_cast<YajlParser *>(ctx)->on(ArrayStartT{});
}

int YajlParser::yajlOnArrayEnd(void *ctx) {
  return static_cast<YajlParser *>(ctx)->on(ArrayEndT{});
}
}  // namespace SJParser
