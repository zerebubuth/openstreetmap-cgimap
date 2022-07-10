#ifndef UTIL_HPP
#define UTIL_HPP

#include "infix_ostream_iterator.hpp"
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

#include <pqxx/array.hxx>


inline size_t unicode_strlen(const std::string & s)
{
   const char* mbstr = s.c_str();

   std::setlocale(LC_ALL, "C.UTF-8");          // TODO: check location for setlocale

   std::mbstate_t state = std::mbstate_t();
   std::size_t len = std::mbsrtowcs(NULL, &mbstr, 0, &state);

   if (len == static_cast<std::size_t> (-1)) {
     throw http::bad_request("Invalid UTF-8 string encountered");
   }

   return len;
}


// TODO: Proper escaping function
inline std::string escape(std::string input)
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


template <typename T>
inline std::string to_string(const std::set<T> &ids) {
  std::stringstream ostr;
  std::copy(ids.begin(), ids.end(),
            infix_ostream_iterator<T>(ostr, ","));
  return ostr.str();
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

	bool operator==(const bbox_t& bbox)
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
};

#endif


