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

#include <vector>

#include "array.h"

namespace SJParser {

/** @brief %Array parser, that stores the result in an std::vector of
 * @ref SArray_T "ParserT" values.
 *
 * @tparam ParserT Underlying parser type.
 * @anchor SArray_T
 */

template <typename ParserT> class SArray : public Array<ParserT> {
 public:
  /** Underlying parser type */
  using ParserType = std::decay_t<ParserT>;

  /** Stored value type */
  using ValueType = std::vector<typename ParserType::ValueType>;

  /** Finish callback type */
  using Callback = std::function<bool(const ValueType &)>;

  /** @brief Constructor.
   *
   * @param [in] parser %Parser for array elements, can be an lvalue reference
   * or an rvalue. It must be one of the parsers that store values (Value,
   * SArray, SAutoObject, SCustomObject, SMap).
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * array is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  explicit SArray(ParserT &&parser, CallbackT on_finish = nullptr);

  /** Move constructor. */
  SArray(SArray &&other) noexcept;

  /** Move assignment operator */
  SArray<ParserT> &operator=(SArray &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~SArray() override = default;
  SArray(const SArray &) = delete;
  SArray &operator=(const SArray &) = delete;
  /** @endcond */

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after the
   * array is parsed.
   *
   * The callback will be called with a reference to the parser as an argument.
   *
   * If the callback returns false, parsing will be stopped with an error.
   */
  void setFinishCallback(Callback on_finish);

  /** @brief Parsed value getter.
   *
   * @return Const reference to a parsed value.
   *
   * @throw std::runtime_error Thrown if the value is unset (no value was
   * parsed or #pop was called).
   */
  [[nodiscard]] const ValueType &get() const;

  /** @brief Get the parsed value and unset the parser.
   *
   * Moves the parsed value out of the parser.
   *
   * @return Rvalue reference to the parsed value.
   *
   * @throw std::runtime_error Thrown if the value is unset (no value was
   * parsed or #pop was called).
   */
  ValueType &&pop();

 private:
  void childParsed() override;
  void finish() override;
  void reset() override;

  ValueType _values;
  Callback _on_finish;
};

template <typename ParserT>
SArray(SArray<ParserT> &&) -> SArray<SArray<ParserT>>;

template <typename ParserT>
SArray(SArray<ParserT> &) -> SArray<SArray<ParserT> &>;

template <typename ParserT> SArray(ParserT &&) -> SArray<ParserT>;

/****************************** Implementations *******************************/

template <typename ParserT>
template <typename CallbackT>
SArray<ParserT>::SArray(ParserT &&parser, CallbackT on_finish)
    : Array<ParserT>{std::forward<ParserT>(parser)},
      _on_finish{std::move(on_finish)} {
  static_assert(std::is_base_of_v<TokenParser, ParserType>,
                "Invalid parser used in SArray");
  static_assert(std::is_constructible_v<Callback, CallbackT>,
                "Invalid callback type");
}

template <typename ParserT>
SArray<ParserT>::SArray(SArray &&other) noexcept
    : Array<ParserT>{std::move(other)},
      _values{std::move(other._values)},
      _on_finish{std::move(other._on_finish)} {}

template <typename ParserT>
SArray<ParserT> &SArray<ParserT>::operator=(SArray &&other) noexcept {
  Array<ParserT>::operator=(std::move(other));
  _values = std::move(other._values);
  _on_finish = std::move(other._on_finish);

  return *this;
}

template <typename ParserT>
void SArray<ParserT>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename ParserT>
const typename SArray<ParserT>::ValueType &SArray<ParserT>::get() const {
  TokenParser::checkSet();
  return _values;
}

template <typename ParserT>
typename SArray<ParserT>::ValueType &&SArray<ParserT>::pop() {
  TokenParser::checkSet();
  TokenParser::unset();
  return std::move(_values);
}

template <typename ParserT> void SArray<ParserT>::childParsed() {
  _values.push_back(Array<ParserT>::parser().pop());
}

template <typename ParserT> void SArray<ParserT>::finish() {
  if (_on_finish && !_on_finish(_values)) {
    throw std::runtime_error("Callback returned false");
  }
}

template <typename ParserT> void SArray<ParserT>::reset() {
  ArrayParser::reset();
  _values = {};
}

}  // namespace SJParser
