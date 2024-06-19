/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef UTIL_HPP
#define UTIL_HPP

#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/options.hpp"

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>

#include <fmt/core.h>
#include <fmt/format.h>


inline size_t unicode_strlen(const std::string & s)
{
   const char* mbstr = s.c_str();

   std::setlocale(LC_ALL, "C.UTF-8");          // TODO: check location for setlocale

   std::mbstate_t state{};
   std::size_t len = std::mbsrtowcs(nullptr, &mbstr, 0, &state);

   if (len == static_cast<std::size_t> (-1)) {
     throw http::bad_request("Invalid UTF-8 string encountered");
   }

   return len;
}


// TODO: Proper escaping function
inline std::string escape(std::string_view input)
{

	std::string result = "\"";

	for (char i : input)
	{
		if (i == '\"')
			result += "\\";
		else if (i == '\\')
			result += "\\";

		result += i;
	}

	result += "\"";

	return result;
}


// array_agg returns some curly brackets in the response. remove them for output
// TODO: find a better way to do this.

inline std::string friendly_name(const std::string & input)
{
  return input.substr(1, input.size() - 2);
}


template <typename T>
inline std::string to_string(const std::set<T> &ids) {
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

	bool operator==(const bbox_t& bbox) const
	{
	  return( minlat == bbox.minlat &&
			  minlon == bbox.minlon &&
			  maxlat == bbox.maxlat &&
			  maxlon == bbox.maxlon);
	}

	friend std::ostream& operator<< (std::ostream& os, const bbox_t& bbox) {
	    os << "[" << bbox.minlat << "," << bbox.minlon << "," << bbox.maxlat << "," << bbox.maxlon << "]";
	    return os;
	}

	long linear_size() const {
	  return ((maxlon - minlon) + (maxlat - minlat));
	}
};

#endif


