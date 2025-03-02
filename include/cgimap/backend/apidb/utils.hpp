/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef CGIMAP_BACKEND_APIDB_UTILS_HPP
#define CGIMAP_BACKEND_APIDB_UTILS_HPP

#include <algorithm>
#include <string_view>
#include <vector>
#include <ranges>

#include <pqxx/pqxx>

/* Unindent SQL statements in raw strings */

#if !defined(__cpp_lib_constexpr_vector)
// fallback implementation for older compilers
// code requires constexpr for std::vector (p1004r2)
consteval const char* operator"" _M(const char* str, size_t len) {
    return str;
}

#else

// multiline_raw_string is based on https://stackoverflow.com/a/75105367/3501889

namespace multiline_raw_string {
    template<class char_type>
    using string_view = std::basic_string_view<char_type>;

    // characters that are considered space
    // we need this because std::isspace is not constexpr
    template<class char_type>
    inline constexpr string_view<char_type> space_chars = std::declval<string_view<char_type>>();
    template<>
    inline constexpr string_view<char> space_chars<char> = " \f\n\r\t\v";

    // list of all potential line endings that could be encountered
    template<class char_type>
    inline constexpr string_view<char_type> potential_line_endings[] = std::declval<string_view<char_type>[]>();
    template<>
    inline constexpr string_view<char> potential_line_endings<char>[] = {
        "\n"
    };


    // null-terminator for the different character types
    template<class char_type>
    inline constexpr char_type null_char = std::declval<char_type>();
    template<>
    inline constexpr char null_char<char> = '\0';

    // returns a view to the leading sequence of space characters within a string
    // e.g. get_leading_space_sequence(" \t  foo") -> " \t  "
    template<class char_type>
    consteval string_view<char_type> get_leading_space_sequence(string_view<char_type> line) {
        return line.substr(0, line.find_first_not_of(space_chars<char_type>));
    }

    // checks if a line consists purely out of space characters
    // e.g. is_line_empty("    \t") -> true
    //      is_line_empty("   foo") -> false
    template<class char_type>
    consteval bool is_line_empty(string_view<char_type> line) {
        return get_leading_space_sequence(line).size() == line.size();
    }

    // splits a string into individual lines
    // and removes the first & last line if they are empty
    // e.g. split_lines("\na\nb\nc\n", "\n") -> {"a", "b", "c"}
    template<class char_type>
    consteval std::vector<string_view<char_type>> split_lines(
        string_view<char_type> str,
        string_view<char_type> line_ending
    ) {
        std::vector<string_view<char_type>> lines;

        for (auto line : std::views::split(str, line_ending)) {
            lines.emplace_back(line.begin(), line.end());
        }

        // remove first/last lines in case they are completely empty
        if(lines.size() > 1 && is_line_empty(lines[0])) {
            lines.erase(lines.begin());
        }
        if(lines.size() > 1 && is_line_empty(lines[lines.size()-1])) {
            lines.erase(lines.end()-1);
        }

        return lines;
    }

    // unindents the individual lines of a raw string literal
    // e.g. unindent_string("  \n  a\n  b\n  c\n") -> "a\nb\nc"
    template<class char_type>
    consteval std::vector<char_type> unindent_string(string_view<char_type> str) {
        string_view<char_type> line_ending = "\n";
        string_view<char_type> space_separator = " ";
        std::vector<string_view<char_type>> lines = split_lines(str, line_ending);

        std::vector<char_type> new_string;
        bool is_first = true;
        for(auto line : lines) {
            // append newline/line separator
            if(is_first) {
                is_first = false;
            } else {
                new_string.insert(new_string.end(), space_separator.begin(), space_separator.end());
            }

            // append unindented line
            auto unindented = line.substr(get_leading_space_sequence(line).size());
            new_string.insert(new_string.end(), unindented.begin(), unindented.end());
        }

        // add null terminator
        new_string.push_back(null_char<char_type>);

        return new_string;
    }

    // returns the size required for the unindented string
    template<class char_type>
    consteval std::size_t unindent_string_size(string_view<char_type> str) {
        return unindent_string(str).size();
    }

    // simple type that stores a raw string
    // we need this to get around the limitation that string literals
    // are not considered valid non-type template arguments.
    template<class _char_type, std::size_t size>
    struct string_wrapper {
        using char_type = _char_type;

        consteval string_wrapper(const char_type (&arr)[size]) {
            std::ranges::copy(arr, str);
        }

        char_type str[size];
    };

    // used for sneakily creating and storing
    // the unindented string in a template parameter.
    template<string_wrapper sw>
    struct unindented_string_wrapper {
        using char_type = typename decltype(sw)::char_type;
        static constexpr std::size_t buffer_size = unindent_string_size<char_type>(sw.str);
        using array_ref = const char_type (&)[buffer_size];

        consteval unindented_string_wrapper(int) {
            auto newstr = unindent_string<char_type>(sw.str);
            std::ranges::copy(newstr, buffer);
        }

        consteval array_ref get() const {
            return buffer;
        }

        char_type buffer[buffer_size];
    };

    // uses a defaulted template argument that depends on the str
    // to initialize the unindented string within a template parameter.
    // this enables us to return a reference to the unindented string.
    template<string_wrapper str, unindented_string_wrapper<str> unindented = 0>
    consteval decltype(auto) do_unindent() {
        return unindented.get();
    }

    // the actual user-defined string literal operator
    template<string_wrapper str>
    consteval decltype(auto) operator"" _M() {
        return do_unindent<str>();
    }
}

using multiline_raw_string::operator"" _M;

#endif

/* checks that the postgres version is sufficient to run cgimap.
 *
 * some queries (e.g: LATERAL join) and functions (multi-parameter unnest) only
 * became available in later versions of postgresql.
 */
void check_postgres_version(pqxx::connection_base &conn);

// parses psql array based on specs given
// https://www.postgresql.org/docs/current/static/arrays.html#ARRAYS-IO
std::vector<std::string> psql_array_to_vector(std::string_view str, int size_hint = 0);
std::vector<std::string> psql_array_to_vector(const pqxx::field& field, int size_hint = 0);

template <typename T>
std::vector<T> psql_array_ids_to_vector(const pqxx::field& field);

template <typename T>
std::vector<T> psql_array_ids_to_vector(std::string_view str);

#endif /* CGIMAP_BACKEND_APIDB_UTILS_HPP */
