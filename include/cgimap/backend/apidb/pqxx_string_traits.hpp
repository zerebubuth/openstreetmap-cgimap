#ifndef BACKEND_APIDB_PQXX_STRING_TRAITS_HPP
#define BACKEND_APIDB_PQXX_STRING_TRAITS_HPP

#include "cgimap/types.hpp"
#include "cgimap/infix_ostream_iterator.hpp"
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include <pqxx/pqxx>

namespace pqxx {

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
      std::stringstream ostr;                                           \
      ostr << "{";                                                      \
      std::copy(ids.begin(), ids.end(),                                 \
                infix_ostream_iterator<type::value_type>(ostr, ","));   \
      ostr << "}";                                                      \
      return ostr.str();                                                \
    }                                                                   \
  }

PQXX_ARRAY_STRING_TRAITS(std::vector<osm_nwr_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::set<osm_nwr_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<tile_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::vector<osm_changeset_id_t>);
PQXX_ARRAY_STRING_TRAITS(std::set<osm_changeset_id_t>);

} // namespace pqxx

#endif /* BACKEND_APIDB_PQXX_STRING_TRAITS_HPP */
