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

#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "default_value.h"
#include "dispatcher.h"
#include "ignore.h"
#include "sjparser/member.h"
#include "sjparser/options.h"
#include "token_parser.h"
#include "traits.h"

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

  using ParserTupleType = std::tuple<ParserTs...>;

  template <std::size_t n>
  using Nth = typename std::tuple_element<n, ParserTupleType>::type;

  template <std::size_t n> using ParserType = std::decay_t<Nth<n>>;

  template <std::size_t n, typename U = ParserType<n>>
  using ValueType = typename U::ValueType;

  template <std::size_t n>
  static constexpr bool has_value_type = IsStorageParser<ParserType<n>>;

  using ParsersMapType = std::unordered_map<InternalNameType, TokenParser *>;

  // Returns ValueType<n> if it is available, otherwise ParserType<n>
  template <size_t n> [[nodiscard]] auto &get();

  template <size_t n> [[nodiscard]] ParserType<n> &parser();

  template <size_t n> ValueType<n> pop();

 protected:
  template <typename ParserT> struct MemberParser {
    MemberParser() = delete;
    MemberParser(const MemberParser &) = delete;
    MemberParser &operator=(const MemberParser &) = delete;
    MemberParser(MemberParser &&other) noexcept = default;
    MemberParser &operator=(MemberParser &&other) noexcept = default;

    explicit MemberParser(Member<NameT, ParserT> &xs)
        : parser(std::forward<ParserT>(xs.parser)),
          name(std::move(xs.name)),
          optional(xs.optional),
          default_value(std::move(xs.default_value)) {}

    ParserT parser;
    NameT name;
    bool optional;
    DefaultValue<ParserT> default_value;
  };

  struct MemberParsers {
    MemberParsers() = delete;
    MemberParsers(const MemberParsers &) = delete;
    MemberParsers &operator=(const MemberParsers &) = delete;
    MemberParsers(MemberParsers &&other) noexcept = default;
    MemberParsers &operator=(MemberParsers &&other) noexcept = default;

    MemberParsers(ParsersMapType &parsers_map,
                  std::tuple<Member<NameT, ParserTs>...> &members)
        : mbr_parsers(to_member_parser_tuple(members)) {
      registerParsers(parsers_map);
    }

    template <size_t n> [[nodiscard]] auto &get() {
      return std::get<n>(mbr_parsers);
    }

    void registerParsers(ParsersMapType &parsers_map);

   private:
    template <typename ParserT>
    void registerParser(ParserT &parser, NameT &name,
                        ParsersMapType &parsers_map);

    void check_duplicate(bool inserted, NameT &name) const;

    [[nodiscard]] auto to_member_parser_tuple(
        std::tuple<Member<NameT, ParserTs>...> &members) {
      return (std::apply(
          [](auto &&...xs) {
            return (std::tuple{MemberParser<ParserTs>(xs)...});
          },
          members));
    }

    std::tuple<MemberParser<ParserTs>...> mbr_parsers;
  };

  [[nodiscard]] auto &parsersMap();
  [[nodiscard]] auto &memberParsers();

 private:
  ParsersMapType _parsers_map;
  MemberParsers _member_parsers;
  Ignore _ignore_parser;
  ObjectOptions _options;
};

/****************************** Implementations *******************************/

template <typename NameT, typename... ParserTs>
KeyValueParser<NameT, ParserTs...>::KeyValueParser(
    std::tuple<Member<NameT, ParserTs>...> members, ObjectOptions options)
    : _member_parsers(_parsers_map, members), _options{options} {}

template <typename NameT, typename... ParserTs>
KeyValueParser<NameT, ParserTs...>::KeyValueParser(
    KeyValueParser &&other) noexcept
    : TokenParser{std::move(other)},
      _member_parsers{std::move(other._member_parsers)},
      _ignore_parser{std::move(other._ignore_parser)},
      _options{other._options} {
  _member_parsers.registerParsers(_parsers_map);
}

template <typename NameT, typename... ParserTs>
KeyValueParser<NameT, ParserTs...> &
KeyValueParser<NameT, ParserTs...>::operator=(KeyValueParser &&other) noexcept {
  TokenParser::operator=(std::move(other));
  _member_parsers = std::move(other._member_parsers);
  _parsers_map.clear();
  _member_parsers.registerParsers(_parsers_map);
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

  if constexpr (has_value_type<n>) {
    if (!member.parser.isSet() && member.default_value.value) {
      return static_cast<
          const typename decltype(member.default_value.value)::value_type &>(
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
typename KeyValueParser<NameT, ParserTs...>::template ParserType<n>
    &KeyValueParser<NameT, ParserTs...>::parser() {
  return _member_parsers.template get<n>().parser;
}

template <typename NameT, typename... ParserTs>
template <size_t n>
typename KeyValueParser<NameT, ParserTs...>::template ValueType<n>
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
auto &KeyValueParser<NameT, ParserTs...>::parsersMap() {
  return _parsers_map;
}

template <typename NameT, typename... ParserTs>
auto &KeyValueParser<NameT, ParserTs...>::memberParsers() {
  return _member_parsers;
}

template <typename NameT, typename... ParserTs>
void KeyValueParser<NameT, ParserTs...>::MemberParsers::registerParsers(
    ParsersMapType &parsers_map) {
  parsers_map.clear();

  std::apply(
      [&](auto &&...xs) {
        ((registerParser(xs.parser, xs.name, parsers_map)), ...);
      },
      mbr_parsers);
}

template <typename NameT, typename... ParserTs>
template <typename ParserT>
void KeyValueParser<NameT, ParserTs...>::MemberParsers::registerParser(
    ParserT &parser, NameT &name, ParsersMapType &parsers_map) {
  auto [_, inserted] = parsers_map.insert({name, &parser});
  std::ignore = _;

  check_duplicate(inserted, name);
}

template <typename NameT, typename... ParserTs>
void KeyValueParser<NameT, ParserTs...>::MemberParsers::check_duplicate(
    bool inserted, NameT &name) const {
  if (!inserted) {
    std::stringstream error;
    error << "Member " << name << " appears more than once";
    throw std::runtime_error(error.str());
  }
}

}  // namespace SJParser
