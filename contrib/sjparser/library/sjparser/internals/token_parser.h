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

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace SJParser {

struct DummyT {};  // not used by yajl
struct NullT {};
struct MapStartT {};
struct MapKeyT {
  std::string_view key;
};
struct MapEndT {};
struct ArrayStartT {};
struct ArrayEndT {};

template <typename TokenT> struct TokenTypeResolver {
  using type = std::decay_t<TokenT>;
};

template <> struct TokenTypeResolver<std::string> {
  using type = std::string_view;
};

template <typename TokenT>
using TokenType = typename TokenTypeResolver<std::decay_t<TokenT>>::type;

// TokenSecondaryTypeResolver is used to indicate that a type (e.g. double)
// should also process yajl events for other data types (e.g. integers)
// Yajl treats numbers without decimal points as integer, and this
// triggers an error in case of Value<double>.
template <typename TokenT> struct TokenSecondaryTypeResolver {
  using type = DummyT;
};

template <> struct TokenSecondaryTypeResolver<double> { using type = int64_t; };

template <typename TokenT>
using TokenSecondaryType =
    typename TokenSecondaryTypeResolver<std::decay_t<TokenT>>::type;

class Dispatcher;

class TokenParser {
 public:
  virtual void setDispatcher(Dispatcher *dispatcher) noexcept;
  [[nodiscard]] bool isSet() const noexcept;
  [[nodiscard]] bool isEmpty() const noexcept;
  virtual void reset();
  void endParsing();
  virtual void finish() = 0;

  virtual void on(NullT unused);
  virtual void on(bool value);
  virtual void on(int64_t value);
  virtual void on(double value);
  virtual void on(std::string_view value);
  virtual void on(MapStartT /*unused*/);
  virtual void on(MapKeyT key);
  virtual void on(MapEndT /*unused*/);
  virtual void on(ArrayStartT /*unused*/);
  virtual void on(ArrayEndT /*unused*/);
  virtual void on(DummyT);

  virtual void childParsed();

  TokenParser() = default;
  virtual ~TokenParser() = default;

  TokenParser(const TokenParser &) = default;
  TokenParser &operator=(const TokenParser &) = default;
  TokenParser(TokenParser &&) = default;
  TokenParser &operator=(TokenParser &&) = default;

 protected:
  inline void checkSet() const;
  inline void unset() noexcept;
  [[noreturn]] static inline void unexpectedToken(const std::string &type);
  inline void setNotEmpty() noexcept;
  [[nodiscard]] inline Dispatcher *dispatcher() noexcept;

 private:
  Dispatcher *_dispatcher = nullptr;
  bool _set = false;
  bool _empty = true;
};

/****************************** Implementations *******************************/

inline bool TokenParser::isSet() const noexcept {
  return _set;
}

inline bool TokenParser::isEmpty() const noexcept {
  return _empty;
}

void TokenParser::checkSet() const {
  if (!isSet()) {
    throw std::runtime_error("Can't get value, parser is unset");
  }
}

void TokenParser::unset() noexcept {
  _set = false;
}

[[noreturn]] void TokenParser::unexpectedToken(const std::string &type) {
  throw std::runtime_error("Unexpected token " + type);
}

void TokenParser::setNotEmpty() noexcept {
  _empty = false;
}

Dispatcher *TokenParser::dispatcher() noexcept {
  return _dispatcher;
}

}  // namespace SJParser
