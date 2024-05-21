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

#include "default_value.h"
#include "dispatcher.h"
#include "ignore.h"
#include "sjparser/member.h"
#include "sjparser/options.h"
#include "token_parser.h"
#include "traits.h"

#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace SJParser {

template <typename NameT, typename... ParserTs>
class KeyValueParser : public TokenParser {
 public:
  using InternalNameType = TokenType<NameT>;

  explicit KeyValueParser(std::tuple<Member<NameT, ParserTs>...> members,
                          ObjectOptions options = {});

  KeyValueParser(KeyValueParser &&other) noexcept;

  KeyValueParser<NameT, ParserTs...> &operator=(
      KeyValueParser &&other) noexcept;

  ~KeyValueParser() override = default;
  KeyValueParser(const KeyValueParser &) = delete;
  KeyValueParser &operator=(const KeyValueParser &) = delete;

  void setDispatcher(Dispatcher *dispatcher) noexcept override;
  void reset() override;

  void on(MapStartT /*unused*/) override;
  void on(MapEndT /*unused*/) override;

  void onMember(InternalNameType member);

  template <size_t n, typename ParserT, typename... ParserTDs> struct NthTypes {
   private:
    using NextLevel = NthTypes<n - 1, ParserTDs...>;

   public:
    using ParserType = typename NextLevel::ParserType;

    template <typename U = NextLevel>
    using ValueType = typename U::template ValueType<>;

    static constexpr bool has_value_type = NextLevel::has_value_type;
  };

  template <typename ParserT, typename... ParserTDs>
  struct NthTypes<0, ParserT, ParserTDs...> {
    using ParserType = std::decay_t<ParserT>;

    template <typename U = ParserType> using ValueType = typename U::ValueType;

    static constexpr bool has_value_type = IsStorageParser<ParserT>;
  };

  template <size_t n>
  using ParserType = typename NthTypes<n, ParserTs...>::ParserType;

  // Returns NthTypes<n, ParserTs...>::template ValueType<> if it is
  // available, otherwise NthTypes<n, ParserTs...>::ParserType
  template <size_t n>[[nodiscard]] auto &get();

  template <size_t n>
  [[nodiscard]] typename NthTypes<n, ParserTs...>::ParserType &parser();

  template <size_t n>
  typename NthTypes<n, ParserTs...>::template ValueType<> pop();

 protected:
  template <size_t n, typename... ParserTDs> struct MemberParser {
    MemberParser(
        std::array<TokenParser *, sizeof...(ParserTs)> & /*parsers_array*/,
        std::unordered_map<InternalNameType, TokenParser *> & /*parsers_map*/,
        std::tuple<Member<NameT, ParserTs>...> & /*members*/) {}

    MemberParser(MemberParser && /*other*/) noexcept {}

    MemberParser<n, ParserTDs...> &operator=(
        MemberParser && /*other*/) noexcept {
      return *this;
    }

    ~MemberParser() = default;
    MemberParser(const MemberParser &other) = delete;
    MemberParser<n, ParserTDs...> &operator=(const MemberParser &other) =
        delete;

    void registerParsers(
        std::array<TokenParser *, sizeof...(ParserTs)> & /*parsers_array*/,
        std::unordered_map<InternalNameType, TokenParser *> & /*parsers_map*/) {
    }
  };

  template <size_t n, typename ParserT, typename... ParserTDs>
  struct MemberParser<n, ParserT, ParserTDs...>
      : private MemberParser<n + 1, ParserTDs...> {
    MemberParser(
        std::array<TokenParser *, sizeof...(ParserTs)> &parsers_array,
        std::unordered_map<InternalNameType, TokenParser *> &parsers_map,
        std::tuple<Member<NameT, ParserTs>...> &members);

    MemberParser(MemberParser &&other) noexcept;

    MemberParser<n, ParserT, ParserTDs...> &operator=(
        MemberParser &&other) noexcept;

    ~MemberParser() = default;
    MemberParser(const MemberParser &other) = delete;
    MemberParser<n, ParserT, ParserTDs...> &operator=(
        const MemberParser &other) = delete;

    void registerParsers(
        std::array<TokenParser *, sizeof...(ParserTs)> &parsers_array,
        std::unordered_map<InternalNameType, TokenParser *> &parsers_map);

    template <size_t index> auto &get();

    ParserT parser;
    NameT name;
    bool optional;
    DefaultValue<ParserT> default_value;
  };

  [[nodiscard]] auto &parsersArray();
  [[nodiscard]] auto &parsersMap();
  [[nodiscard]] auto &memberParsers();

 private:
  std::array<TokenParser *, sizeof...(ParserTs)> _parsers_array;
  std::unordered_map<InternalNameType, TokenParser *> _parsers_map;
  MemberParser<0, ParserTs...> _member_parsers;
  Ignore _ignore_parser;
  ObjectOptions _options;
};

/****************************** Implementations *******************************/

template <typename NameT, typename... ParserTs>
KeyValueParser<NameT, ParserTs...>::KeyValueParser(
    std::tuple<Member<NameT, ParserTs>...> members, ObjectOptions options)
    : _member_parsers{_parsers_array, _parsers_map, members},
      _options{options} {}

template <typename NameT, typename... ParserTs>
KeyValueParser<NameT, ParserTs...>::KeyValueParser(
    KeyValueParser &&other) noexcept
    : TokenParser{std::move(other)},
      _member_parsers{std::move(other._member_parsers)},
      _ignore_parser{std::move(other._ignore_parser)},
      _options{other._options} {
  _member_parsers.registerParsers(_parsers_array, _parsers_map);
}

template <typename NameT, typename... ParserTs>
KeyValueParser<NameT, ParserTs...> &KeyValueParser<NameT, ParserTs...>::
operator=(KeyValueParser &&other) noexcept {
  TokenParser::operator=(std::move(other));
  _member_parsers = std::move(other._member_parsers);
  _parsers_map.clear();
  _member_parsers.registerParsers(_parsers_array, _parsers_map);
  _ignore_parser = std::move(other._ignore_parser);
  _options = std::move(other._options);

  return *this;
}

template <typename NameT, typename... ParserTs>
void KeyValueParser<NameT, ParserTs...>::setDispatcher(
    Dispatcher *dispatcher) noexcept {
  TokenParser::setDispatcher(dispatcher);
  for (auto &member : _parsers_map) {
    member.second->setDispatcher(dispatcher);
  }

  _ignore_parser.setDispatcher(dispatcher);
}

template <typename NameT, typename... ParserTs>
void KeyValueParser<NameT, ParserTs...>::reset() {
  TokenParser::reset();

  for (auto &member : _parsers_map) {
    member.second->reset();
  }

  _ignore_parser.reset();
}

template <typename NameT, typename... ParserTs>
void KeyValueParser<NameT, ParserTs...>::on(MapStartT /*unused*/) {
  reset();
}

template <typename NameT, typename... ParserTs>
void KeyValueParser<NameT, ParserTs...>::on(MapEndT /*unused*/) {
  endParsing();
}

template <typename NameT, typename... ParserTs>
void KeyValueParser<NameT, ParserTs...>::onMember(TokenType<NameT> member) {
  setNotEmpty();

  if (auto parser_it = _parsers_map.find(member);
      parser_it != _parsers_map.end()) {
    dispatcher()->pushParser(parser_it->second);
    return;
  }

  if (_options.unknown_member == Reaction::Error) {
    std::stringstream error;
    error << "Unexpected member " << member;
    throw std::runtime_error(error.str());
  }

  dispatcher()->pushParser(&_ignore_parser);
}

template <typename NameT, typename... ParserTs>
template <size_t n>
auto &KeyValueParser<NameT, ParserTs...>::get() {
  auto &member = _member_parsers.template get<n>();

  if constexpr (NthTypes<n, ParserTs...>::has_value_type) {
    if (!member.parser.isSet() && member.default_value.value) {
      return static_cast<const typename decltype(
          member.default_value.value)::value_type &>(
          *member.default_value.value);
    }
    return member.parser.get();
    // Disable readability-else-after-return
  } else {  // NOLINT
    return member.parser;
  }
}

template <typename NameT, typename... ParserTs>
template <size_t n>
typename KeyValueParser<NameT, ParserTs...>::template NthTypes<
    n, ParserTs...>::ParserType &
KeyValueParser<NameT, ParserTs...>::parser() {
  return _member_parsers.template get<n>().parser;
}

template <typename NameT, typename... ParserTs>
template <size_t n>
typename KeyValueParser<NameT, ParserTs...>::template NthTypes<
    n, ParserTs...>::template ValueType<>
KeyValueParser<NameT, ParserTs...>::pop() {
  auto &member = _member_parsers.template get<n>();

  if (!member.parser.isSet() && member.default_value.value) {
    // This form is used to reduce the number of required methods for
    // SCustomObject stored type. Otherwise a copy constructor would be
    // necessary.
    typename decltype(member.default_value.value)::value_type value;
    value = *member.default_value.value;
    return value;
  }
  return std::move(member.parser.pop());
}

template <typename NameT, typename... ParserTs>
auto &KeyValueParser<NameT, ParserTs...>::parsersArray() {
  return _parsers_array;
}

template <typename NameT, typename... ParserTs>
auto &KeyValueParser<NameT, ParserTs...>::parsersMap() {
  return _parsers_map;
}

template <typename NameT, typename... ParserTs>
auto &KeyValueParser<NameT, ParserTs...>::memberParsers() {
  return _member_parsers;
}

template <typename NameT, typename... ParserTs>
template <size_t n, typename ParserT, typename... ParserTDs>
KeyValueParser<NameT, ParserTs...>::MemberParser<n, ParserT, ParserTDs...>::
    MemberParser(
        std::array<TokenParser *, sizeof...(ParserTs)> &parsers_array,
        std::unordered_map<InternalNameType, TokenParser *> &parsers_map,
        std::tuple<Member<NameT, ParserTs>...> &members)
    : MemberParser<n + 1, ParserTDs...>{parsers_array, parsers_map, members},
      parser{std::forward<ParserT>(std::get<n>(members).parser)},
      name{std::move(std::get<n>(members).name)},
      optional{std::get<n>(members).optional},
      default_value{std::move(std::get<n>(members).default_value)} {
  parsers_array[n] = &parser;

  auto [_, inserted] = parsers_map.insert({name, &parser});
  std::ignore = _;
  if (!inserted) {
    std::stringstream error;
    error << "Member " << name << " appears more, than once";
    throw std::runtime_error(error.str());
  }
}

template <typename NameT, typename... ParserTs>
template <size_t n, typename ParserT, typename... ParserTDs>
KeyValueParser<NameT, ParserTs...>::MemberParser<
    n, ParserT, ParserTDs...>::MemberParser(MemberParser &&other) noexcept
    : MemberParser<n + 1, ParserTDs...>{std::move(other)},
      parser{std::forward<ParserT>(other.parser)},
      name{std::move(other.name)},
      optional{other.optional},
      default_value{std::move(other.default_value)} {}

template <typename NameT, typename... ParserTs>
template <size_t n, typename ParserT, typename... ParserTDs>
typename KeyValueParser<NameT, ParserTs...>::template MemberParser<
    n, ParserT, ParserTDs...> &
KeyValueParser<NameT, ParserTs...>::MemberParser<n, ParserT, ParserTDs...>::
operator=(MemberParser &&other) noexcept {
  MemberParser<n + 1, ParserTDs...>::operator=(std::move(other));
  parser = std::forward<ParserT>(other.parser);
  name = std::move(other.name);
  optional = other.optional;
  default_value = std::move(other.default_value);

  return *this;
}

template <typename NameT, typename... ParserTs>
template <size_t n, typename ParserT, typename... ParserTDs>
void KeyValueParser<NameT, ParserTs...>::
    MemberParser<n, ParserT, ParserTDs...>::registerParsers(
        std::array<TokenParser *, sizeof...(ParserTs)> &parsers_array,
        std::unordered_map<InternalNameType, TokenParser *> &parsers_map) {
  MemberParser<n + 1, ParserTDs...>::registerParsers(parsers_array,
                                                     parsers_map);
  parsers_array[n] = &parser;
  parsers_map[name] = &parser;
}

template <typename NameT, typename... ParserTs>
template <size_t n, typename ParserT, typename... ParserTDs>
template <size_t index>
auto &KeyValueParser<NameT, ParserTs...>::MemberParser<n, ParserT,
                                                       ParserTDs...>::get() {
  if constexpr (index == n) {
    return *this;
    // Disable readability-else-after-return
  } else {  // NOLINT
    return MemberParser<n + 1, ParserTDs...>::template get<index>();
  }
}

}  // namespace SJParser
