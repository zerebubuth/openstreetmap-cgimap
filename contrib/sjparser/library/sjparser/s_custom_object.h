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
#include "type_holder.h"

namespace SJParser {

/** @brief %Object parser, that stores parsed value of type
 * @ref SCustomObject_TypeT "TypeT"
 *
 * Stored value is set from a finish callback.
 *
 * All object members are mandatory, except for members with Presence::Optional.
 * For absent members with default value SCustomObject::get<> will return the
 * default value and isSet will return false.
 *
 * Unknown members will cause parsing error unless ObjectOptions is passed to
 * the constructor with unknown_member set to Reaction::Ignore.
 *
 * Empty object will be parsed and marked as unset.
 *
 * @tparam TypeT Stored value type. It must have a default constructor and a
 * copy constructor. If you want to include this parser into SArray, a move
 * constructor of this type will be used if possible. If you want to use move
 * assignment operator on this parser, this type should have copy of move
 * assignment operator.
 * @anchor SCustomObject_TypeT
 *
 * @tparam ParserTs A list of member parsers types.
 */

template <typename TypeT, typename... ParserTs>
class SCustomObject : public Object<ParserTs...> {
 public:
  /** Stored value type */
  using ValueType = TypeT;

  /** Finish callback type. */
  using Callback =
      std::function<bool(SCustomObject<ValueType, ParserTs...> &, ValueType &)>;

  /** @brief Constructor.
   *
   * @param [in] type Stored value type, please use a TypeHolder wrapper for it.
   *
   * @param [in] members std::tuple of Member structures, describing object
   * members.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * object is parsed.
   * The callback will be called with a reference to the parser and a reference
   * to the stored value as arguments. If the callback returns false, parsing
   * will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  SCustomObject(TypeHolder<ValueType> type,
                std::tuple<Member<std::string, ParserTs>...> members,
                CallbackT on_finish = nullptr,
                std::enable_if_t<std::is_constructible_v<Callback, CallbackT>>
                    * /*unused*/
                = nullptr);

  /** @brief Constructor.
   *
   * @param [in] type Stored value type, please use a TypeHolder wrapper for it.
   *
   * @param [in] members std::tuple of Member structures, describing object
   * members.
   *
   * @param [in] options Additional options for the parser.
   *
   * @param [in] on_finish (optional) Callback, that will be called after the
   * object is parsed.
   * The callback will be called with a reference to the parser and a reference
   * to the stored value as arguments. If the callback returns false, parsing
   * will be stopped with an error.
   */
  template <typename CallbackT = std::nullptr_t>
  SCustomObject(TypeHolder<ValueType> type,
                std::tuple<Member<std::string, ParserTs>...> members,
                ObjectOptions options, CallbackT on_finish = nullptr);

  /** Move constructor. */
  SCustomObject(SCustomObject &&other) noexcept;

  /** Move assignment operator */
  SCustomObject<TypeT, ParserTs...> &operator=(SCustomObject &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~SCustomObject() override = default;
  SCustomObject(const SCustomObject &) = delete;
  SCustomObject &operator=(const SCustomObject &) = delete;
  /** @endcond */

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after the
   * object is parsed.
   * The callback will be called with a reference to the parser and a reference
   * to the stored value as arguments. If the callback returns false, parsing
   * will be stopped with an error.
   */
  void setFinishCallback(Callback on_finish);

  using Object<ParserTs...>::get;

  /** @brief Parsed value getter.
   *
   * @return Const reference to a parsed value.
   *
   * @throw std::runtime_error Thrown if the value is unset (no value was
   * parsed or #pop was called).
   */
  [[nodiscard]] const ValueType &get() const;

  using Object<ParserTs...>::pop;

  /** @brief Get the parsed value and unset the parser.
   *
   * Moves the parsed value out of the parser.
   *
   * @note
   * @par
   * If you want your SCustomObject::ValueType to be movable, you need to
   * provide either a move assignment operator or a copy assignment operator,
   * they are used internally.
   * @par
   * If you want to use this parser in the SAutoObject, you need to provide both
   * a copy constructor and a copy assignment operator in the
   * SCustomObject::ValueType, they are used by std::tuple.
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

  ValueType _value;
  Callback _on_finish;
};

/****************************** Implementations *******************************/

template <typename TypeT, typename... ParserTs>
template <typename CallbackT>
SCustomObject<TypeT, ParserTs...>::SCustomObject(
    TypeHolder<TypeT> type,
    std::tuple<Member<std::string, ParserTs>...> members, CallbackT on_finish,
    std::enable_if_t<std::is_constructible_v<Callback, CallbackT>> * /*unused*/)
    : SCustomObject{type, std::move(members), ObjectOptions{},
                    std::move(on_finish)} {}

template <typename TypeT, typename... ParserTs>
template <typename CallbackT>
SCustomObject<TypeT, ParserTs...>::SCustomObject(
    TypeHolder<TypeT> /*type*/,
    std::tuple<Member<std::string, ParserTs>...> members, ObjectOptions options,
    CallbackT on_finish)
    : Object<ParserTs...>{std::move(members), options},
      _on_finish{std::move(on_finish)} {
  static_assert(std::is_constructible_v<Callback, CallbackT>,
                "Invalid callback type");
}

template <typename TypeT, typename... ParserTs>
SCustomObject<TypeT, ParserTs...>::SCustomObject(SCustomObject &&other) noexcept
    : Object<ParserTs...>{std::move(other)},
      _value{std::move(other._value)},
      _on_finish{std::move(other._on_finish)} {}

template <typename TypeT, typename... ParserTs>
SCustomObject<TypeT, ParserTs...> &SCustomObject<TypeT, ParserTs...>::operator=(
    SCustomObject &&other) noexcept {
  Object<ParserTs...>::operator=(std::move(other));
  _value = std::move(other._value);
  _on_finish = std::move(other._on_finish);

  return *this;
}

template <typename TypeT, typename... ParserTs>
void SCustomObject<TypeT, ParserTs...>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename TypeT, typename... ParserTs>
const typename SCustomObject<TypeT, ParserTs...>::ValueType &
SCustomObject<TypeT, ParserTs...>::get() const {
  TokenParser::checkSet();
  return _value;
}

template <typename TypeT, typename... ParserTs>
typename SCustomObject<TypeT, ParserTs...>::ValueType &&
SCustomObject<TypeT, ParserTs...>::pop() {
  TokenParser::checkSet();
  TokenParser::unset();
  return std::move(_value);
}

template <typename TypeT, typename... ParserTs>
void SCustomObject<TypeT, ParserTs...>::finish() {
  if (TokenParser::isEmpty()) {
    TokenParser::unset();
    return;
  }

  try {
    typename Object<ParserTs...>::template MemberChecker<0, ParserTs...>(*this);
  } catch (std::exception &) {
    TokenParser::unset();
    throw;
  }

  if (_on_finish && !_on_finish(*this, _value)) {
    throw std::runtime_error("Callback returned false");
  }
}

template <typename TypeT, typename... ParserTs>
void SCustomObject<TypeT, ParserTs...>::reset() {
  Object<ParserTs...>::KVParser::reset();
  _value = {};
}

}  // namespace SJParser
