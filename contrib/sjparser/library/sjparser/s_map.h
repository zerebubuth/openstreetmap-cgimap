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

#include <map>

#include "map.h"

namespace SJParser {

/** @brief %Map parser, that stores the result in an std::map with std::string
 * as a key and @ref SMap_T "ParserT" as a value.
 *
 * Parses an object where each member value has type @ref SMap_T "ParserT",
 * and member name represents map key.
 *
 * @tparam ParserT Element's value parser type.
 * @anchor SMap_T
 */

template <typename ParserT> class SMap : public Map<ParserT> {
 public:
  /** Element's value parser type. */
  using ParserType = std::decay_t<ParserT>;

  /** Stored value type */
  using ValueType = std::map<std::string, typename ParserType::ValueType>;

  /** Element callback type. */
  using ElementCallback =
      std::function<bool(const std::string &, ParserType &)>;

  /** Finish callback type. */
  using Callback = std::function<bool(SMap<ParserT> &)>;

  /** @brief Constructor.
   *
   * @param [in] parser %Parser for map elements values, can be an lvalue
   * reference or an rvalue. It must be one of the parsers that store values
   * (Value, SArray, SAutoObject, SCustomObject, SMap).
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * map is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  explicit SMap(ParserT &&parser, CallbackT on_finish = nullptr,
                std::enable_if_t<std::is_constructible_v<Callback, CallbackT>>
                    * /*unused*/
                = nullptr);

  /** @brief Constructor.
   *
   * @param [in] parser %Parser for map elements values, can be an lvalue
   * reference or an rvalue. It must be one of the parsers that store values
   * (Value, SArray, SAutoObject, SCustomObject, SMap).
   *
   * @param [in] on_element Callback, that will be called after an element of
   * the map is parsed.
   * The callback will be called with a map key and a reference to the parser as
   * arguments.
   * If the callback returns false, parsing will be stopped with an error.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * array is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename ElementCallbackT, typename CallbackT = std::nullptr_t>
  SMap(ParserT &&parser, ElementCallbackT on_element,
       CallbackT on_finish = nullptr);

  /** Move constructor. */
  SMap(SMap &&other) noexcept;

  /** Move assignment operator */
  SMap<ParserT> &operator=(SMap &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~SMap() override = default;
  SMap(const SMap &) = delete;
  SMap &operator=(const SMap &) = delete;
  /** @endcond */

  /** @brief Element callback setter.
   *
   * @param [in] on_element Callback, that will be called after an element of
   * the map is parsed.
   * The callback will be called with a map key and a reference to the parser as
   * arguments.
   * If the callback returns false, parsing will be stopped with an error.
   * */
  void setElementCallback(ElementCallback on_element);

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after the
   * map is parsed.
   * The callback will be called with a reference to the parser as an argument.
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
  ElementCallback _on_element;
  Callback _on_finish;
};

template <typename ParserT> SMap(SMap<ParserT> &&) -> SMap<SMap<ParserT>>;

template <typename ParserT> SMap(SMap<ParserT> &) -> SMap<SMap<ParserT> &>;

template <typename ParserT> SMap(ParserT &&) -> SMap<ParserT>;

/****************************** Implementations *******************************/

template <typename ParserT>
template <typename CallbackT>
SMap<ParserT>::SMap(
    ParserT &&parser, CallbackT on_finish,
    std::enable_if_t<std::is_constructible_v<Callback, CallbackT>> * /*unused*/)
    : SMap{std::forward<ParserT>(parser), nullptr, std::move(on_finish)} {}

template <typename ParserT>
template <typename ElementCallbackT, typename CallbackT>
SMap<ParserT>::SMap(ParserT &&parser, ElementCallbackT on_element,
                    CallbackT on_finish)
    : Map<ParserT>{std::forward<ParserT>(parser)},
      _on_element{std::move(on_element)},
      _on_finish{std::move(on_finish)} {
  static_assert(std::is_base_of_v<TokenParser, ParserType>,
                "Invalid parser used in Map");
  static_assert(std::is_constructible_v<Callback, CallbackT>,
                "Invalid callback type");
}

template <typename ParserT>
SMap<ParserT>::SMap(SMap &&other) noexcept
    : Map<ParserT>{std::move(other)},
      _values{std::move(other._values)},
      _on_element{std::move(other._on_element)},
      _on_finish{std::move(other._on_finish)} {}

template <typename ParserT>
SMap<ParserT> &SMap<ParserT>::operator=(SMap &&other) noexcept {
  Map<ParserT>::operator=(std::move(other));
  _values = std::move(other._values);
  _on_element = std::move(other._on_element);
  _on_finish = std::move(other._on_finish);

  return *this;
}

template <typename ParserT>
void SMap<ParserT>::setElementCallback(ElementCallback on_element) {
  _on_element = on_element;
}

template <typename ParserT>
void SMap<ParserT>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename ParserT>
const typename SMap<ParserT>::ValueType &SMap<ParserT>::get() const {
  TokenParser::checkSet();
  return _values;
}

template <typename ParserT>
typename SMap<ParserT>::ValueType &&SMap<ParserT>::pop() {
  TokenParser::checkSet();
  TokenParser::unset();
  return std::move(_values);
}

template <typename ParserT> void SMap<ParserT>::childParsed() {
  if (_on_element
      && !_on_element(Map<ParserT>::currentKey(), Map<ParserT>::parser())) {
    throw std::runtime_error("Element callback returned false");
  }
  _values.insert_or_assign(Map<ParserT>::currentKey(),
                           Map<ParserT>::parser().pop());
}

template <typename ParserT> void SMap<ParserT>::finish() {
  if (_on_finish && !_on_finish(*this)) {
    throw std::runtime_error("Callback returned false");
  }
}

template <typename ParserT> void SMap<ParserT>::reset() {
  TokenParser::reset();
  _values = {};
}

}  // namespace SJParser
