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

#include <type_traits>

#include "internals/default_value.h"
#include "internals/traits.h"

namespace SJParser {

/** Enum with member presence options */
enum class Presence {
  /** Optional member */
  Optional
};

/** @brief Member of object parsers specification.
 *
 * This structure holds a specification of individual member for object parsers.
 *
 * @tparam NameT Type of member name
 *
 * @tparam ParserT Type of member parser
 */
template <typename NameT, typename ParserT> struct Member {
  /** Member name */
  NameT name;
  /** Member parser */
  ParserT parser;

  /** Is this member optional */
  bool optional = false;

  /** Default value */
  DefaultValue<ParserT> default_value;

  /** @brief Constructor
   *
   * @param [in] name Member name.
   *
   * @param [in] parser Member parser, can be an lvalue reference or an rvalue.
   */
  Member(NameT name, ParserT &&parser);

  /** @brief Constructor
   *
   * Creates an optional member.
   *
   * @param [in] name Member name.
   *
   * @param [in] parser Member parser, can be an lvalue reference or an rvalue.
   *
   * @param [in] presence Presence enum, indicates that this is an optional
   * member.
   */
  Member(NameT name, ParserT &&parser, Presence presence);

  /** @brief Constructor
   *
   * Creates an optional member with a default value, can be used only with
   * storage parsers.
   *
   * @param [in] name Member name.
   *
   * @param [in] parser Member parser, can be an lvalue reference or an rvalue.
   *
   * @param [in] presence Presence enum, indicates that this is an optional
   * member.
   *
   * @param [in] default_value Default value.
   */
  template <typename ParserU = std::decay_t<ParserT>>
  Member(NameT name, ParserT &&parser, Presence presence,
         typename ParserU::ValueType default_value);

  /** Move constructor. */
  Member(Member &&other) noexcept;

  /** Move assignment operator */
  Member<NameT, ParserT> &operator=(Member &&other) noexcept;

  /** @cond INTERNAL Boilerplate. */
  ~Member() = default;
  Member(const Member &) = delete;
  Member &operator=(const Member &) = delete;
  /** @endcond */

 private:
  constexpr void checkTemplateParameters();
};

template <typename ParserT>
Member(const char *, ParserT &&) -> Member<std::string, ParserT>;

template <typename ParserT> Member(int, ParserT &&) -> Member<int64_t, ParserT>;

template <typename ParserT>
Member(float, ParserT &&) -> Member<double, ParserT>;

template <typename NameT, typename ParserT>
Member(NameT, ParserT &&) -> Member<NameT, ParserT>;

template <typename ParserT>
Member(const char *, ParserT &&, Presence) -> Member<std::string, ParserT>;

template <typename ParserT>
Member(int, ParserT &&, Presence) -> Member<int64_t, ParserT>;

template <typename ParserT>
Member(float, ParserT &&, Presence) -> Member<double, ParserT>;

template <typename NameT, typename ParserT>
Member(NameT, ParserT &&, Presence) -> Member<NameT, ParserT>;

template <typename ParserT, typename ValueT>
Member(const char *, ParserT &&, Presence, ValueT)
    -> Member<std::string, ParserT>;

template <typename ParserT, typename ValueT>
Member(int, ParserT &&, Presence, ValueT) -> Member<int64_t, ParserT>;

template <typename ParserT, typename ValueT>
Member(float, ParserT &&, Presence, ValueT) -> Member<double, ParserT>;

template <typename NameT, typename ParserT, typename ValueT>
Member(NameT, ParserT &&, Presence, ValueT) -> Member<NameT, ParserT>;

/****************************** Implementations *******************************/

template <typename NameT, typename ParserT>
Member<NameT, ParserT>::Member(NameT name, ParserT &&parser)
    : name{std::move(name)}, parser{std::forward<ParserT>(parser)} {
  checkTemplateParameters();
}

template <typename NameT, typename ParserT>
Member<NameT, ParserT>::Member(NameT name, ParserT &&parser,
                               Presence /*presence*/)
    : name{std::move(name)},
      parser{std::forward<ParserT>(parser)},
      optional{true} {
  checkTemplateParameters();
}

template <typename NameT, typename ParserT>
template <typename ParserU>
Member<NameT, ParserT>::Member(NameT name, ParserT &&parser,
                               Presence /*presence*/,
                               typename ParserU::ValueType default_value)
    : name{std::move(name)},
      parser{std::forward<ParserT>(parser)},
      optional{true},
      default_value{std::move(default_value)} {
  checkTemplateParameters();
}

template <typename NameT, typename ParserT>
Member<NameT, ParserT>::Member(Member &&other) noexcept
    : name{std::move(other.name)},
      parser{std::forward<ParserT>(other.parser)},
      optional{other.optional},
      default_value{std::move(other.default_value)} {}

template <typename NameT, typename ParserT>
Member<NameT, ParserT> &Member<NameT, ParserT>::operator=(
    Member &&other) noexcept {
  name = std::move(other.name);
  parser = std::forward<ParserT>(other.parser);
  optional = other.optional;
  default_value = std::move(other.default_value);

  return *this;
}

template <typename NameT, typename ParserT>
constexpr void Member<NameT, ParserT>::checkTemplateParameters() {
  // Formatting disabled because of a bug in clang-format
  // clang-format off
  static_assert(
      std::is_same_v<NameT, int64_t>
      || std::is_same_v<NameT, bool>
      || std::is_same_v<NameT, double>
      || std::is_same_v<NameT, std::string>,
      "Invalid type used for Member name, only int64_t, bool, double or "
      "std::string are allowed");
  // clang-format on
  static_assert(std::is_base_of_v<TokenParser, std::decay_t<ParserT>>,
                "Invalid parser used in Member");
}

}  // namespace SJParser
