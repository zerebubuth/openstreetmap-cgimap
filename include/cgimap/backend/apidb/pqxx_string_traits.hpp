/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef BACKEND_APIDB_PQXX_STRING_TRAITS_HPP
#define BACKEND_APIDB_PQXX_STRING_TRAITS_HPP

#include <algorithm>
#include <set>
#include <sstream>
#include <vector>

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

template<typename Container>
struct array_string_traits
{
private:
  using elt_type = strip_t<value_type<Container>>;
  using elt_traits = string_traits<elt_type>;

public:
  static zview to_buf(char *begin, char *end, Container const &value) = delete;

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
      return 3 + std::accumulate(    // +3 for curly braces and null byte at the end
                   std::begin(value), std::end(value), std::size_t{},
                   [](std::size_t acc, elt_type const &elt) {
                     return acc + elt_traits::size_buffer(elt) + 1;  // +1 for comma separator between elements
                   });
  }

};

}

#define PQXX_ARRAY_STRING_TRAITS(type)                                  \
  template <>                                                           \
  struct nullness<type> : no_null<type> {};                             \
                                                                        \
  template<>                                                            \
  struct string_traits<type> : cgimap::array_string_traits<type> {};    \
                                                                        \
  template<> inline std::string const type_name<type>{#type};           \


#endif

PQXX_ARRAY_STRING_TRAITS(std::vector<osm_nwr_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::set<osm_nwr_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<tile_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<osm_changeset_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::set<osm_changeset_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<std::string>);

} // namespace pqxx

#endif /* BACKEND_APIDB_PQXX_STRING_TRAITS_HPP */
