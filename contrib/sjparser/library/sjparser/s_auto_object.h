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

#include "object.h"

namespace SJParser {

/** @brief %Object parser, that stores the result in an std::tuple of member
 * parsers types.
 *
 * All object members are mandatory, except for members with Presence::Optional
 * and default value set.
 *
 * Unknown members will cause parsing error unless ObjectOptions is passed to
 * the constructor with unknown_member set to Reaction::Ignore.
 *
 * Empty object will be parsed and marked as unset.
 *
 * @tparam ParserTs A list of member parsers types.
 */

template <typename... ParserTs> class SAutoObject : public Object<ParserTs...> {
 public:
  /** Stored value type */
  using ValueType = std::tuple<typename std::decay_t<ParserTs>::ValueType...>;

  /** Finish callback type. */
  using Callback = std::function<bool(const ValueType &)>;

  /** @brief Constructor.
   *
   * @param [in] members std::tuple of Member structures, describing object
   * members.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * object is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  explicit SAutoObject(
      std::tuple<Member<std::string_view, ParserTs>...> members,
      CallbackT on_finish = nullptr)
      requires std::is_constructible_v<Callback, CallbackT>;

  /** @brief Constructor.
   *
   * @param [in] members std::tuple of Member structures, describing object
   * members.
   *
   * @param [in] options Additional options for the parser.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * object is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  SAutoObject(std::tuple<Member<std::string_view, ParserTs>...> members,
              ObjectOptions options, CallbackT on_finish = nullptr)
              requires std::is_constructible_v<Callback, CallbackT>;

  /** Move constructor. */
  SAutoObject(SAutoObject &&other) noexcept;

  /** Move assignment operator */
  SAutoObject<ParserTs...> &operator=(SAutoObject &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~SAutoObject() override = default;
  SAutoObject(const SAutoObject &) = delete;
  SAutoObject &operator=(const SAutoObject &) = delete;
  /** @endcond */

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after the
   * object is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  void setFinishCallback(Callback on_finish);

  /** @brief Parsed value getter.
   *
   * @return Const reference to a parsed value or a default value (if a default
   * value is set and the member is not present).
   *
   * @throw std::runtime_error Thrown if the value is unset (no value was
   * parsed and no default value was specified or #pop was called).
   */
  [[nodiscard]] const ValueType &get() const;

  /** @brief Get the parsed value and unset the parser.
   *
   * Moves the parsed value out of the parser.
   *
   * @note If you want to use SCustomObject inside this parser, you need to
   * provide both a copy constructor or move constructor and a copy assignment
   * operators in the SCustomObject::ValueType, they are used by parser.
   *
   * @return Rvalue reference to the parsed value.
   *
   * @throw std::runtime_error Thrown if the value is unset (no value was
   * parsed or #pop was called).
   */
  ValueType &&pop();

 private:
  void finish() override;
  void reset() override;

  // This is placed in the private section because the ValueSetter uses pop on
  // all members, so they are always unset after parsing.
  using Object<ParserTs...>::get;
  using Object<ParserTs...>::pop;

  void valueSetter(ValueType& _value);

  template <size_t n>
  void valueSetter(ValueType& _value, auto& parser);

  ValueType _value;
  Callback _on_finish;
};

/****************************** Implementations *******************************/

template <typename... ParserTs>
template <typename CallbackT>
SAutoObject<ParserTs...>::SAutoObject(
    std::tuple<Member<std::string_view, ParserTs>...> members, CallbackT on_finish)
    requires std::is_constructible_v<Callback, CallbackT>
    : SAutoObject{std::move(members), ObjectOptions{}, std::move(on_finish)} {}

template <typename... ParserTs>
template <typename CallbackT>
SAutoObject<ParserTs...>::SAutoObject(
    std::tuple<Member<std::string_view, ParserTs>...> members, ObjectOptions options,
    CallbackT on_finish)
    requires std::is_constructible_v<Callback, CallbackT>
    : Object<ParserTs...>{std::move(members), options},
      _on_finish{std::move(on_finish)} {}

template <typename... ParserTs>
SAutoObject<ParserTs...>::SAutoObject(SAutoObject &&other) noexcept
    : Object<ParserTs...>{std::move(other)},
      _value{std::move(other._value)},
      _on_finish{std::move(other._on_finish)} {}

template <typename... ParserTs>
SAutoObject<ParserTs...> &SAutoObject<ParserTs...>::operator=(
    SAutoObject &&other) noexcept {
  Object<ParserTs...>::operator=(std::move(other));
  _value = std::move(other._value);
  _on_finish = std::move(other._on_finish);

  return *this;
}

template <typename... ParserTs>
void SAutoObject<ParserTs...>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename... ParserTs>
const typename SAutoObject<ParserTs...>::ValueType &
SAutoObject<ParserTs...>::get() const {
  TokenParser::checkSet();
  return _value;
}

template <typename... ParserTs>
typename SAutoObject<ParserTs...>::ValueType &&SAutoObject<ParserTs...>::pop() {
  TokenParser::checkSet();
  TokenParser::unset();
  return std::move(_value);
}

template <typename... ParserTs> void SAutoObject<ParserTs...>::finish() {
  if (TokenParser::isEmpty()) {
    TokenParser::unset();
    return;
  }

  try {
    valueSetter(_value);
  } catch (std::exception &e) {
    TokenParser::unset();
    throw std::runtime_error(std::string("Can not set value: ") + e.what());
  } catch (...) {
    TokenParser::unset();
    throw std::runtime_error("Can not set value: unknown exception");
  }

  if (_on_finish && !_on_finish(_value)) {
    throw std::runtime_error("Callback returned false");
  }
}

template <typename... ParserTs> void SAutoObject<ParserTs...>::reset() {
  Object<ParserTs...>::KVParser::reset();
  _value = {};
}

template <typename... ParserTs>
void SAutoObject<ParserTs...>::valueSetter(ValueType &value) {
  auto &parsers = this->memberParsers().parsers();
  [&]<std::size_t... Indices>(std::index_sequence<Indices...>) {
    std::apply(
        [&](auto &&...member) { ((valueSetter<Indices>(value, member)), ...); },
        parsers);
  }(std::index_sequence_for<ParserTs...>{});
}

template <typename... ParserTs>
template <size_t n>
void SAutoObject<ParserTs...>::valueSetter(ValueType &value, auto &member) {
  if (member.parser.isSet()) {
    std::get<n>(value) = member.parser.pop();
  } else if (member.optional) {
    if (!member.default_value.value) {
      throw std::runtime_error("Optional member " + std::string(member.name)
                               + " does not have a default value");
    }
    std::get<n>(value) = *member.default_value.value;
  } else {
    throw std::runtime_error("Mandatory member " + std::string(member.name)
                             + " is not present");
  }
}

}  // namespace SJParser
