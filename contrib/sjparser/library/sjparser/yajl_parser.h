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

#include <yajl/yajl_parse.h>

#include <memory>
#include <string>

#include "internals/dispatcher.h"
#include "parsing_error.h"

namespace SJParser {

/** @brief YAJL parser adapter.
 *
 */
class YajlParser {
 public:
  /** Constructor */
  YajlParser();

  /** @cond INTERNAL Boilerplate. */
  ~YajlParser() = default;
  YajlParser(const YajlParser &) = delete;
  YajlParser &operator=(const YajlParser &) = delete;
  YajlParser(YajlParser &&) = delete;
  YajlParser &operator=(YajlParser &&) = delete;
  /** @endcond */

  /** @brief Parses a piece of JSON from an std::string.
   *
   * @param [in] data String to parse.
   *
   * @return True if parsing was successful and false otherwise.
   *
   * @throw ParsingError Thrown in case of parsing problems.
   *
   * @throw std::runtime_error Thrown if yajl memory allocation fails.
   *
   * @note
   * @par
   * After ParsingError exception is thrown the parser will be reset, so you can
   * use it again.
   * @par
   * After any other exception is thrown the state of this parser is undefined,
   * any you must not use it.
   */
  void parse(const std::string &data);

  /** @brief Parses a piece of JSON from a C-style string.
   *
   * @param [in] data Pointer to a C string to parse.
   *
   * @param [in] len String length.
   *
   * @throw ParsingError Thrown in case of parsing problems.
   *
   * @throw std::runtime_error Thrown if yajl memory allocation fails.
   *
   * @note
   * @par
   * After ParsingError exception is thrown the parser will be reset, so you can
   * use it again.
   * @par
   * After any other exception is thrown the state of this parser is undefined,
   * any you must not use it.
   */
  void parse(const char *data, size_t len);

  /** @brief Finishes parsing.
   *
   * @throw ParsingError Thrown in case of parsing problems.
   *
   * @note
   * @par
   * After ParsingError exception is thrown the parser will be reset, so you can
   * use it again.
   * @par
   * After any other exception is thrown the state of this parser is undefined,
   * any you must not use it.
   */
  void finish();

 protected:
  /** @cond INTERNAL Token parser setter */
  void setTokenParser(TokenParser *parser);
  /** @endcond */

 private:
  void resetYajlHandle();
  void freeYajlHandle();
  void checkDispatcherStack() const;
  [[noreturn]] void throwParsingError();

  template <typename TokenT> int on(TokenT token) noexcept;

  static int yajlOnNull(void *ctx);
  static int yajlOnBool(void *ctx, int value);
  // Disable google-runtime-int long long -> int64_t
  static int yajlOnInt(void *ctx, long long value);  // NOLINT
  static int yajlOnDouble(void *ctx, double value);
  static int yajlOnString(void *ctx, const unsigned char *value, size_t len);
  static int yajlOnMapStart(void *ctx);
  static int yajlOnMapKey(void *ctx, const unsigned char *value, size_t len);
  static int yajlOnMapEnd(void *ctx);
  static int yajlOnArrayStart(void *ctx);
  static int yajlOnArrayEnd(void *ctx);

  std::unique_ptr<Dispatcher> _dispatcher;
  std::unique_ptr<yajl_handle_t, std::function<decltype(yajl_free)>>
      _yajl_handle;
  const unsigned char *_data = nullptr;
  size_t _len = 0;
  bool _reset_needed = false;
  std::string _sjparser_error;

  static constexpr yajl_callbacks _parser_yajl_callbacks = {
      yajlOnNull,   yajlOnBool,       yajlOnInt,      yajlOnDouble,
      nullptr,      yajlOnString,     yajlOnMapStart, yajlOnMapKey,
      yajlOnMapEnd, yajlOnArrayStart, yajlOnArrayEnd};
};

/****************************** Implementations *******************************/

template <typename TokenT> int YajlParser::on(TokenT token) noexcept {
  try {
    _dispatcher->on(token);
  } catch (std::exception &e) {
    _sjparser_error = e.what();
    return 0;
  } catch (...) {
    _sjparser_error = "Unknown exception";
    return 0;
  }
  return 1;
}

}  // namespace SJParser
