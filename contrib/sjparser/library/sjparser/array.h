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

#include <functional>

#include "internals/array_parser.h"

namespace SJParser {

/** @brief %Array parser.
 *
 * @tparam ParserT Elements parser type.
 */

template <typename ParserT> class Array : public ArrayParser {
 public:
  /** Elements parser type. */
  using ParserType = std::decay_t<ParserT>;

  /** Finish callback type. */
  using Callback = std::function<bool(Array<ParserT> &)>;

  /** @brief Constructor.
   *
   * @param [in] parser %Parser for array elements, can be an lvalue reference
   * or an rvalue.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * array is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  explicit Array(ParserT &&parser, CallbackT on_finish = nullptr);

  /** Move constructor. */
  Array(Array &&other) noexcept;

  /** Move assignment operator */
  Array<ParserT> &operator=(Array &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~Array() override = default;
  Array(const Array &) = delete;
  Array &operator=(const Array &) = delete;
  /** @endcond */

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after the
   * array is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  void setFinishCallback(Callback on_finish);

  /** @brief Elements parser getter.
   *
   * @return Reference to the elements parser.
   */
  [[nodiscard]] ParserType &parser();

 private:
  void finish() override;

  ParserT _parser;
  Callback _on_finish;
};

template <typename ParserT> Array(Array<ParserT> &&) -> Array<Array<ParserT>>;

template <typename ParserT> Array(Array<ParserT> &) -> Array<Array<ParserT> &>;

template <typename ParserT> Array(ParserT &&) -> Array<ParserT>;

/****************************** Implementations *******************************/

template <typename ParserT>
template <typename CallbackT>
Array<ParserT>::Array(ParserT &&parser, CallbackT on_finish)
    : _parser{std::forward<ParserT>(parser)}, _on_finish{std::move(on_finish)} {
  static_assert(std::is_base_of_v<TokenParser, ParserType>,
                "Invalid parser used in Array");
  static_assert(std::is_constructible_v<Callback, CallbackT>,
                "Invalid callback type");
  setParserPtr(&_parser);
}

template <typename ParserT>
Array<ParserT>::Array(Array &&other) noexcept
    : ArrayParser{std::move(other)},
      _parser{std::forward<ParserT>(other._parser)},
      _on_finish{std::move(other._on_finish)} {
  setParserPtr(&_parser);
}

template <typename ParserT>
Array<ParserT> &Array<ParserT>::operator=(Array &&other) noexcept {
  ArrayParser::operator=(std::move(other));
  _parser = std::forward<ParserT>(other._parser);
  setParserPtr(&_parser);
  _on_finish = std::move(other._on_finish);

  return *this;
}

template <typename ParserT>
void Array<ParserT>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename ParserT>
typename Array<ParserT>::ParserType &Array<ParserT>::parser() {
  return _parser;
}

template <typename ParserT> void Array<ParserT>::finish() {
  if (_on_finish && !_on_finish(*this)) {
    throw std::runtime_error("Callback returned false");
  }
}

}  // namespace SJParser
