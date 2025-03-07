/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef UTIL_HPP
#define UTIL_HPP

#include "cgimap/http.hpp"
#include "cgimap/options.hpp"

#include <algorithm>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <ostream>
#include <string>
#include <string_view>
#include <ranges>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>


#if __APPLE__
// NOTE: <codecvt> is deprecated in C++17 and removed in C++26 (see P2871R3).

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"

#include <codecvt>
#include <iomanip>
#include <stdexcept>

inline std::size_t unicode_strlen(const std::string& utf8)
{

  try {
    return std::wstring_convert< std::codecvt_utf8<char32_t>, char32_t >{}.from_bytes(utf8).size();
  } catch(std::range_error) {
    throw http::bad_request("Invalid UTF-8 string encountered");
  }
}

#pragma clang diagnostic pop

#else

inline size_t unicode_strlen(const std::string & s)
{
   const char* mbstr = s.c_str();

   std::mbstate_t state{};
   std::size_t len = std::mbsrtowcs(nullptr, &mbstr, 0, &state);

   if (len == static_cast<std::size_t> (-1)) {
     throw http::bad_request("Invalid UTF-8 string encountered");
   }

   return len;
}

#endif

inline char tolower_ascii(char c) {

  if (c >= 'A' && c <= 'Z') {
    return c + ('a' - 'A');
  }
  return c;
}

inline bool ichar_equals(char a, char b) {
  return a == b ||
      tolower_ascii(static_cast<unsigned char>(a)) ==
      tolower_ascii(static_cast<unsigned char>(b));
}

// Case insensitive string comparison
inline bool iequals(std::string_view a, std::string_view b) {
  return a.size() == b.size() &&
         std::ranges::equal(a, b, ichar_equals);
}

template <typename T>
concept StringLike = std::is_same_v<std::remove_cvref_t<T>, std::string> ||
                     std::is_same_v<std::remove_cvref_t<T>, std::string_view>;

template <StringLike T>
inline T trim(T str) {
  auto start = str.find_first_not_of(" \t\n\r");
  if (start == T::npos)
      return {};
  auto end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

template <StringLike T>
inline std::vector<T> split_trim(T str, char delim) {

  auto split_view = str | std::views::split(delim);

  std::vector<T> result;
  for (auto&& part : split_view) {
      auto trimmed = trim(T(&*part.begin(), std::ranges::distance(part)));
      if (!trimmed.empty()) {
          result.push_back(trimmed);
      }
  }
  return result;
}

template <StringLike T>
inline std::vector<T> split(T str, char delim) {
    auto split_result = str | std::ranges::views::split(delim);

    std::vector<T> result;
    for (auto&& part : split_result) {
        result.push_back(T(&*part.begin(), std::ranges::distance(part)));
    }

    return result;
}

template <typename T> T parse_ruby_number(std::string_view str) {

  T id{};

  auto [_, ec] = std::from_chars(str.data(), str.data() + str.size(), id);

  if (ec != std::errc()) {
    // note that this doesn't really make sense without understanding that
    // "some_string".to_i = 0 in ruby
    return {};
  }
  return id;
}

inline std::string escape(std::string_view input) {

  int n = 0;

  // count chars to be escaped
  for (char c : input) {
    if (c == '"' || c == '\\')
      ++n;
  }

  std::string result(input.size() + n + 2, ' ');   // input size + # of escaped chars + 2 enclosing quotes

  int offset = 0;

  result[offset++] = '"';

  for (char c : input) {
    if (c == '"' || c == '\\')
      result[offset++] = '\\';

    result[offset++] = c;
  }

  result[offset++] = '"';

  assert(result.size() == offset);

  return result;
}

template <typename T>
inline std::string to_string(const T &ids) {
  return fmt::format("{}", fmt::join(ids, ","));
}


// Bounding box
class bbox_t {

public:
	long minlat;
	long minlon;
	long maxlat;
	long maxlon;

	bbox_t() : minlat(200 * global_settings::get_scale()),
	           minlon(200 * global_settings::get_scale()),
		   maxlat(-200 * global_settings::get_scale()),
		   maxlon(-200 * global_settings::get_scale()) {};

	bbox_t(double _minlat, double _minlon, double _maxlat, double _maxlon) :
	  minlat(_minlat * global_settings::get_scale()),
	  minlon(_minlon * global_settings::get_scale()),
	  maxlat(_maxlat * global_settings::get_scale()),
	  maxlon(_maxlon * global_settings::get_scale()) {};

	void expand(const bbox_t& bbox)
	{
		minlat = std::min(minlat, bbox.minlat);
		minlon = std::min(minlon, bbox.minlon);
		maxlat = std::max(maxlat, bbox.maxlat);
		maxlon = std::max(maxlon, bbox.maxlon);
	}

	bool operator==(const bbox_t& bbox) const = default;

	friend std::ostream& operator<< (std::ostream& os, const bbox_t& bbox) {
	    os << "[" << bbox.minlat << "," << bbox.minlon << "," << bbox.maxlat << "," << bbox.maxlon << "]";
	    return os;
	}

	long linear_size() const {
	  return ((maxlon - minlon) + (maxlat - minlat));
	}
};

#endif


