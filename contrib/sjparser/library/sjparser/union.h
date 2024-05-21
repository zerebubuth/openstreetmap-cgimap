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

#include "internals/key_value_parser.h"
#include "type_holder.h"

#include <functional>

namespace SJParser {

/** @brief %Union of @ref Object "Objects" parser.
 *
 * Parses an object from @ref Union_Ts "ParserTs" list based on a value of
 * the type member.
 *
 * You can use it standalone (in this case the first member of an object must
 * be a type member) or embedded in an object (in this case object members after
 * the type member will be parsed by one of union's object parsers).
 *
 * Union type is defined by arguments passed to the constructor.
 *
 * Empty standalone union will be parsed and marked as unset.
 *
 * If union type was parsed, then the corresponding object is mandatory
 * unless it's member has Presence::Optional set.
 * For absent member with default value Union::get<> will return the default
 * value and isSet will return false.
 *
 * @tparam TypeMemberT A type of the type member. Can be int64_t, bool, double
 * or std::string.
 *
 * @tparam ParserTs A list of object parsers.
 * @anchor Union_Ts
 */

template <typename TypeMemberT, typename... ParserTs>
class Union : public KeyValueParser<TypeMemberT, ParserTs...> {
 protected:
  /** @cond INTERNAL Internal typedef */
  using KVParser = KeyValueParser<TypeMemberT, ParserTs...>;
  /** @endcond */

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

  /** Finish callback type. */
  using Callback = std::function<bool(Union<TypeMemberT, ParserTs...> &)>;

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
  Union(TypeHolder<TypeMemberT> type,
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
  Union(TypeHolder<TypeMemberT> type, std::string_view type_member,
        std::tuple<Member<TypeMemberT, ParserTs>...> members,
        CallbackT on_finish = nullptr);

  /** Move constructor. */
  Union(Union &&other) noexcept;

  /** Move assignment operator */
  Union<TypeMemberT, ParserTs...> &operator=(Union &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~Union() override = default;
  Union(const Union &) = delete;
  Union &operator=(const Union &) = delete;
  /** @endcond */

  /** @brief Finish callback setter.
   *
   * @param [in] on_finish Callback, that will be called after the
   * union is parsed.
   * The callback will be called with a reference to the parser as an argument.
   * If the callback returns false, parsing will be stopped with an error.
   */
  void setFinishCallback(Callback on_finish);

  /** @brief Parsed object index getter.
   *
   * @return Index of a parsed object.
   *
   * @throw std::runtime_error Thrown if no members were parsed.
   */
  [[nodiscard]] size_t currentMemberId();

#ifdef DOXYGEN_ONLY
  /** @brief Check if the parser has a value.
   *
   * @return True if the parser parsed something or false otherwise.
   */
  [[nodiscard]] bool isSet();

  /** @brief Check if the parsed union was empy (null).
   *
   * @return True if the parsed union was empty (null) or false otherwise.
   */
  [[nodiscard]] bool isEmpty();
#endif

#ifdef DOXYGEN_ONLY
  /** @brief Universal member getter.
   *
   * @tparam n Index of the parser's member.
   *
   * @return If the n-th member parser stores value (is a Value, SAutoObject,
   * SCustomObject, SUnion or SArray), then the method returns a const reference
   * to the n-th member parser value. Otherwise, returns a reference to the n-th
   * member parser.
   *
   * @throw std::runtime_error thrown if the member parser value is unset (no
   * value was parsed or #pop was called for the member parser).
   */
  [[nodiscard]] template <size_t n> auto &get();

  /** @brief Member parser getter.
   *
   * @tparam n Index of the parser's member.
   *
   * @return Reference to n-th member parser.
   */
  [[nodiscard]] template <size_t n>
  typename NthTypes<n, ParserTs...>::ParserType &parser();

  /** @brief Get the member parsed value and unset the member parser.
   *
   * Moves the n-th member parsed value out of the member parser.
   *
   * @tparam n Index of the parser's member.
   *
   * @return Rvalue reference to n-th member parser value.
   *
   * @throw std::runtime_error thrown if the member parser value is unset (no
   * value was parsed or #pop was called for the member parser).
   */
  template <size_t n>
  typename NthTypes<n, ParserTs...>::template ValueType<> &&pop();
#endif

 protected:
  void reset() override;

 private:
  void setupIdsMap();

  using KVParser::on;

  void on(TokenType<TypeMemberT> value) override;
  void on(MapStartT /*unused*/) override;
  void on(MapKeyT key) override;

  void childParsed() override;
  void finish() override;

  template <size_t, typename...> struct MemberChecker {
    explicit MemberChecker(Union<TypeMemberT, ParserTs...> & /*parser*/) {}
  };

  template <size_t n, typename ParserT, typename... ParserTDs>
  struct MemberChecker<n, ParserT, ParserTDs...>
      : private MemberChecker<n + 1, ParserTDs...> {
    explicit MemberChecker(Union<TypeMemberT, ParserTs...> &parser);
  };

  std::string _type_member;
  Callback _on_finish;
  std::unordered_map<TokenParser *, size_t> _members_ids_map;
  size_t _current_member_id = 0;
};

/****************************** Implementations *******************************/

template <typename TypeMemberT, typename... ParserTs>
template <typename CallbackT>
Union<TypeMemberT, ParserTs...>::Union(
    TypeHolder<TypeMemberT> type,
    std::tuple<Member<TypeMemberT, ParserTs>...> members, CallbackT on_finish)
    : Union{type, {}, std::move(members), std::move(on_finish)} {}

template <typename TypeMemberT, typename... ParserTs>
template <typename CallbackT>
Union<TypeMemberT, ParserTs...>::Union(
    TypeHolder<TypeMemberT> /*type*/, std::string_view type_member,
    std::tuple<Member<TypeMemberT, ParserTs>...> members, CallbackT on_finish)
    : KVParser{std::move(members), {}},
      _type_member{type_member},
      _on_finish{std::move(on_finish)} {
  static_assert(std::is_constructible_v<Callback, CallbackT>,
                "Invalid callback type");
  setupIdsMap();
}

template <typename TypeMemberT, typename... ParserTs>
Union<TypeMemberT, ParserTs...>::Union(Union &&other) noexcept
    : KVParser{std::move(other)},
      _type_member{std::move(other._type_member)},
      _on_finish{std::move(other._on_finish)},
      _current_member_id(other._current_member_id) {
  setupIdsMap();
}

template <typename TypeMemberT, typename... ParserTs>
Union<TypeMemberT, ParserTs...> &Union<TypeMemberT, ParserTs...>::operator=(
    Union &&other) noexcept {
  KVParser::operator=(std::move(other));
  _type_member = std::move(other._type_member);
  _on_finish = std::move(other._on_finish);
  _current_member_id = other._current_member_id;
  setupIdsMap();

  return *this;
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::setupIdsMap() {
  _members_ids_map.clear();
  size_t index = 0;
  for (const auto &id : KVParser::parsersArray()) {
    _members_ids_map[id] = index;
    ++index;
  }
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::setFinishCallback(Callback on_finish) {
  _on_finish = on_finish;
}

template <typename TypeMemberT, typename... ParserTs>
size_t Union<TypeMemberT, ParserTs...>::currentMemberId() {
  TokenParser::checkSet();
  return _current_member_id;
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::reset() {
  _current_member_id = 0;
  KVParser::reset();
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::on(TokenType<TypeMemberT> value) {
  reset();
  KVParser::onMember(value);
  _current_member_id = _members_ids_map[KVParser::parsersMap()[value]];
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::on(MapStartT /*unused*/) {
  if (_type_member.empty()) {
    throw std::runtime_error(
        "Union with an empty type member can't parse this");
  }
  reset();
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::on(MapKeyT key) {
  if (_type_member.empty()) {
    throw std::runtime_error(
        "Union with an empty type member can't parse this");
  }
  if (key.key != _type_member) {
    std::stringstream err;
    err << "Unexpected member " << key.key;
    throw std::runtime_error(err.str());
  }
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::childParsed() {
  KVParser::endParsing();
  if (_type_member.empty()) {
    // The union embedded into an object must propagate the end event to the
    // parent.
    TokenParser::dispatcher()->on(MapEndT());
  }
}

template <typename TypeMemberT, typename... ParserTs>
void Union<TypeMemberT, ParserTs...>::finish() {
  if (TokenParser::isEmpty()) {
    TokenParser::unset();
    return;
  }

  try {
    MemberChecker<0, ParserTs...>(*this);
  } catch (std::exception &e) {
    TokenParser::unset();
    throw;
  }

  if (_on_finish && !_on_finish(*this)) {
    throw std::runtime_error("Callback returned false");
  }
}

template <typename TypeMemberT, typename... ParserTs>
template <size_t n, typename ParserT, typename... ParserTDs>
Union<TypeMemberT, ParserTs...>::MemberChecker<n, ParserT, ParserTDs...>::
    MemberChecker(Union<TypeMemberT, ParserTs...> &parser)
    : MemberChecker<n + 1, ParserTDs...>{parser} {
  if (parser.currentMemberId() != n) {
    return;
  }

  auto &member = parser.memberParsers().template get<n>();

  if (!member.parser.isSet() && !member.optional) {
    std::stringstream error;
    error << "Mandatory member #" << n << " is not present";
    throw std::runtime_error(error.str());
  }
}

}  // namespace SJParser
