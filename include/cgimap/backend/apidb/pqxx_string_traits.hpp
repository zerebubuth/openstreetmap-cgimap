/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#ifndef BACKEND_APIDB_PQXX_STRING_TRAITS_HPP
#define BACKEND_APIDB_PQXX_STRING_TRAITS_HPP

#include <algorithm>
#include <iterator>
#include <set>
#include <string_view>
#include <vector>
#include <ranges>

#include <fmt/core.h>
#include <fmt/format.h>

#include <pqxx/pqxx>

#include "cgimap/types.hpp"

namespace pqxx {

#if PQXX_VERSION_MAJOR < 7
/*
 * PQXX_ARRAY_STRING_TRAITS provides an instantiation of the string_traits
 * template from PQXX which is used to stringify arguments when sending them
 * to Postgres. Cgimap uses several different containers across different
 * integer types, all of which stringify to arrays in the same way.
 *
 * Note that it would be nicer to hide this in a .cpp, but it seems that the
 * implementation has to be available when used in the prepared statement
 * code.
 */
#define PQXX_ARRAY_STRING_TRAITS(type)                                  \
  template <> struct string_traits<type> {                              \
    static const char *name() { return #type; }                         \
    static bool has_null() { return false; }                            \
    static bool is_null(const type &) { return false; }                 \
    static std::stringstream null() {                                   \
      internal::throw_null_conversion(name());                          \
      throw 0; /* need this to satisfy compiler escape detection */     \
    }                                                                   \
    static void from_string(const char[], type &) {}                    \
    static std::string to_string(const type &ids) {                     \
      return fmt::format("{{{}}}", fmt::join(ids, ","));                \
    }                                                                   \
  }

#else

// See https://github.com/jtv/libpqxx/blob/7.7/include/pqxx/doc/datatypes.md

namespace cgimap {

// !! cgimap::array_string_traits expects any strings to be already escaped !!

// libpqxx 8 provides built-in string serialization functions for ranges, so
// providing our own here is technically no longer necessary. However, using the
// built-in libpqxx functions currently leads to string values being quoted
// twice, since the existing cgimap range string serialization functions
// expect strings to be pre-escaped, while the built-in libpqxx functions
// perform quoting internally.
//
// libpqxx v8.0.1 also still has a bug in the built-in serialization
// functions for ranges of numbers, which means they cannot be used safely
// (https://github.com/jtv/libpqxx/issues/1235)

template<std::ranges::range Container>
struct array_string_traits
{
private:
  using elt_type = std::remove_cvref_t<value_type<Container>>;
  using elt_traits = string_traits<elt_type>;

public:
  static zview to_buf(char *begin, char *end, Container const &value) = delete;

#if PQXX_VERSION_MAJOR >= 8
  [[nodiscard]] static inline std::string_view
  to_buf(std::span<char> buf, Container const &value, ctx c = {})
  {
    auto const budget{ size_buffer(value) };

    if (std::cmp_less(std::size(buf), budget))
      throw pqxx::conversion_overrun{
        "Not enough buffer space to convert array to string.", c.loc
      };

    std::size_t here{ 0u };
    buf[here++] = '{';

    bool nonempty{ false };
    for (auto const &elt : value)
    {
      // This serialization function doesn't work for nullable types
      static_assert(!pqxx::nullness<elt_type>::has_null);

      auto elt_buf = buf.subspan(here);
      auto out = elt_traits::to_buf(elt_buf, elt, c);
      auto sz = std::size(out);
      auto available_len = std::size(elt_buf);

      if (std::cmp_greater(sz, available_len))
        throw pqxx::conversion_overrun{
          std::format(
              "Buffer too small to convert {} value to string (needs a {}-byte "
              "buffer).",
              name_type<elt_type>(), sz),
          c.loc
        };
      // no-op if out.data() == buf.data() + here
      std::memmove(buf.data() + here, out.data(), sz);
      here += sz;

      buf[here++] = array_separator<elt_type>;
      nonempty = true;
    }

    // Erase that last comma, if present.
    if (nonempty)
      here--;

    buf[here++] = '}';

    return { std::data(buf), here };
  }
#endif

  static char *into_buf(char *begin, char *end, Container const &value)
  {
    auto const budget{size_buffer(value)};
    if (static_cast<std::size_t>(std::distance(begin, end)) < budget)
      throw conversion_overrun{
        "Not enough buffer space to convert container to string."};

    char *here = begin;
    *here++ = '{';

    bool nonempty{false};
    for (auto const &elt : value)
    {
      here = elt_traits::into_buf(here, end, elt) - 1;
      *here++ = array_separator<elt_type>;
      nonempty = true;
    }

    // Erase that last comma, if present.
    if (nonempty)
      here--;

    *here++ = '}';
    *here++ = '\0';

    return here;
  }

  static std::size_t size_buffer(Container const &value) noexcept
  {
      // Assume that no escaping is required (strings already need to be pre-escaped)!
      return 3 + std::accumulate(    // +3 for curly braces and null byte at the end
                   std::begin(value), std::end(value), std::size_t{},
                   [](std::size_t acc, elt_type const &elt) {
                     return acc + elt_traits::size_buffer(elt) + 1;  // +1 for comma separator between elements
                   });
  }

};

}

// Provide a definition of the pqxx::name_type() function template for backward
// compatibility if it's missing
#if PQXX_VERSION_MAJOR < 8
template<typename T> inline constexpr std::string_view name_type() noexcept
{
  return type_name<T>;
}
#endif

#define PQXX_ARRAY_STRING_TRAITS(type)                                  \
  template <>                                                           \
  struct nullness<type> : no_null<type> {};                             \
                                                                        \
  template<>                                                            \
  struct string_traits<type> : cgimap::array_string_traits<type> {};    \
                                                                        \
  template<> inline std::string const type_name<type>{#type};           \
  template<> inline constexpr std::string_view                          \
  name_type<type>() noexcept { return #type; };


#endif

PQXX_ARRAY_STRING_TRAITS(std::vector<osm_nwr_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::set<osm_nwr_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<tile_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<osm_changeset_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::set<osm_changeset_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<std::string>);

} // namespace pqxx

#endif /* BACKEND_APIDB_PQXX_STRING_TRAITS_HPP */
