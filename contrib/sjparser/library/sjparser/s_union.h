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

#include "union.h"

#include <functional>
#include <variant>

namespace SJParser {

/** @brief %Union of @ref Object "Objects" parser, that stores the result in an
 * std::variant of member parser types.
 *
 * Parses an object from @ref SUnion_Ts "ParserTs" list based on a value of
 * the type member.
 *
 * You can use it standalone (in this case the first member of an object must
 * be a type member) or embedded in an object (in this case object members after
 * the type member will be parsed by one of union's object parsers).
 *
 * SUnion type is defined by arguments passed to the constructor.
 *
 * Empty standalone union will be parsed and marked as unset.
 *
 * If union type was parsed, then the corresponding object is mandatory
 * unless it's member has Presence::Optional and default value set.
 *
 * @tparam TypeMemberT A type of the type member. Can be int64_t, bool, double
 * or std::string.
 *
 * @tparam ParserTs A list of object parsers.
 * @anchor SUnion_Ts
 */

template <typename TypeMemberT, typename... ParserTs>
class SUnion : public Union<TypeMemberT, ParserTs...> {
 public:
#ifdef DOXYGEN_ONLY
  /** @brief %Member parser type.
   *
   * Resolves to n-th member parser type
   *
   * @tparam n %Member index
   */
  template <size_t n> struct ParserType {
    /** n-th member parser type */
    using ParserType = NthTypes<n, ParserTDs...>::ParserType;
  };
#endif

  /** Stored value type */
  using ValueType = std::variant<typename std::decay_t<ParserTs>::ValueType...>;

  /** Finish callback type. */
  using Callback = std::function<bool(const ValueType &)>;

  /** @brief Embedded mode constructor.
   *
   * This union must be used as a member of an object. In JSON that member's
   * content will represent this union's type. Latter members will be parsed by
   * one of this union's objects parsers.
   *
   * @param [in] type Type member's type, please use a TypeHolder wrapper for
   * it.
   *
   * @param [in] members std::tuple of Member structures, describing union
   * objects.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * union is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  SUnion(TypeHolder<TypeMemberT> type,
         std::tuple<Member<TypeMemberT, ParserTs>...> members,
         CallbackT on_finish = nullptr);

  /** @brief Standalone mode constructor.
   *
   * This union will parse a whole JSON object. Object's first member's name
   * must be same as type_member, and latter members will be parsed by one of
   * this union's objects parsers.
   *
   * @param [in] type Type member's type, please use a TypeHolder wrapper for
   * it.
   *
   * @param [in] type_member Type member's name.
   *
   * @param [in] members std::tuple of Member structures, describing union
   * objects.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * union is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  SUnion(TypeHolder<TypeMemberT> type, std::string_view type_member,
         std::tuple<Member<TypeMemberT, ParserTs>...> members,
         CallbackT on_finish = nullptr);

  /** Move constructor. */
  SUnion(SUnion &&other) noexcept;

  /** Move assignment operator */
  SUnion<TypeMemberT, ParserTs...> &operator=(SUnion &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~SUnion() override = default;
  SUnion(const SUnion &) = delete;
  SUnion &operator=(const SUnion &) = delete;
  /** @endcond */

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after the
   * union is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  void setFinishCallback(Callback on_finish);

#ifdef DOXYGEN_ONLY
  /** @brief Check if the parser has a value.
   *
   * @return True if the parser has some value stored or false otherwise.
   */
  [[nodiscard]] bool isSet();

  /** @brief Check if the parsed union was empy (null).
   *
   * @return True if the parsed union was empty (null) or false otherwise.
   */
  [[nodiscard]] bool isEmpty();
#endif

  /** @brief Parsed value getter.
   *
   * @return Const reference to a parsed value.
   *
   * @throw std::runtime_error Thrown if the value is unset (no value was
   * parsed or #pop was called).
   */
  [[nodiscard]] const ValueType &get() const;

#ifdef DOXYGEN_ONLY
  /** @brief Member parser getter.
   *
   * @tparam n Index of the parser's member.
   *
   * @return Reference to n-th member parser.
   */
  [[nodiscard]] template <size_t n>
  typename NthTypes<n, ParserTs...>::ParserType &parser();
#endif

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
  using Union<TypeMemberT, ParserTs...>::get;
  using Union<TypeMemberT, ParserTs...>::pop;
  using Union<TypeMemberT, ParserTs...>::currentMemberId;

  template <size_t, typename...> struct ValueSetter {
    ValueSetter(ValueType & /*value*/,
                SUnion<TypeMemberT, ParserTs...> & /*parser*/) {}
  };

  template <size_t n, typename ParserT, typename... ParserTDs>
  struct ValueSetter<n, ParserT, ParserTDs...>
      : private ValueSetter<n + 1, ParserTDs...> {
    ValueSetter(ValueType &value, SUnion<TypeMemberT, ParserTs...> &parser);
  };

  ValueType _value;
  Callback _on_finish;
};

/****************************** Implementations *******************************/

template <typename TypeMemberT, typename... ParserTs>
template <typename CallbackT>
SUnion<TypeMemberT, ParserTs...>::SUnion(
    TypeHolder<TypeMemberT> type,
    std::tuple<Member<TypeMemberT, ParserTs>...> members, CallbackT on_finish)
    : SUnion{type, {}, std::move(members), std::move(on_finish)} {}

template <typename TypeMemberT, typename... ParserTs>
template <typename CallbackT>
SUnion<TypeMemberT, ParserTs...>::SUnion(
    TypeHolder<TypeMemberT> type, std::string_view type_member,
    std::tuple<Member<TypeMemberT, ParserTs>...> members, CallbackT on_finish)
    : Union<TypeMemberT, ParserTs...>{type, type_member, std::move(members)},
      _on_finish{std::move(on_finish)} {}

template <typename TypeMemberT, typename... ParserTs>
SUnion<TypeMemberT, ParserTs...>::SUnion(SUnion &&other) noexcept
    : Union<TypeMemberT, ParserTs...>{std::move(other)},
      _value{std::move(other._value)},
      _on_finish{std::move(other._on_finish)} {}

template <typename TypeMemberT, typename... ParserTs>
SUnion<TypeMemberT, ParserTs...> &SUnion<TypeMemberT, ParserTs...>::operator=(
    SUnion &&other) noexcept {
  Union<TypeMemberT, ParserTs...>::operator=(std::move(other));
  _value = std::move(other._value);
  _on_finish = std::move(other._on_finish);

  return *this;
}

template <typename TypeMemberT, typename... ParserTs>
void SUnion<TypeMemberT, ParserTs...>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename TypeMemberT, typename... ParserTs>
const typename SUnion<TypeMemberT, ParserTs...>::ValueType &
SUnion<TypeMemberT, ParserTs...>::get() const {
  TokenParser::checkSet();
  return _value;
}

template <typename TypeMemberT, typename... ParserTs>
typename SUnion<TypeMemberT, ParserTs...>::ValueType &&
SUnion<TypeMemberT, ParserTs...>::pop() {
  TokenParser::checkSet();
  TokenParser::unset();
  return std::move(_value);
}

template <typename TypeMemberT, typename... ParserTs>
void SUnion<TypeMemberT, ParserTs...>::finish() {
  if (TokenParser::isEmpty()) {
    TokenParser::unset();
    return;
  }

  try {
    ValueSetter<0, ParserTs...>(_value, *this);
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

template <typename TypeMemberT, typename... ParserTs>
void SUnion<TypeMemberT, ParserTs...>::reset() {
  Union<TypeMemberT, ParserTs...>::reset();
  _value = {};
}

template <typename TypeMemberT, typename... ParserTs>
template <size_t n, typename ParserT, typename... ParserTDs>
SUnion<TypeMemberT, ParserTs...>::ValueSetter<n, ParserT, ParserTDs...>::
    ValueSetter(ValueType &value, SUnion<TypeMemberT, ParserTs...> &parser)
    : ValueSetter<n + 1, ParserTDs...>{value, parser} {
  if (parser.currentMemberId() != n) {
    return;
  }

  auto &member = parser.memberParsers().template get<n>();

  if (member.parser.isSet()) {
    value = member.parser.pop();
  } else if (member.optional) {
    if (!member.default_value.value) {
      std::stringstream error;
      error << "Optional member #" << n << " does not have a default value";
      throw std::runtime_error(error.str());
    }
    value = *member.default_value.value;
  } else {
    std::stringstream error;
    error << "Mandatory member #" << n << " is not present";
    throw std::runtime_error(error.str());
  }
}

}  // namespace SJParser
