#ifndef UTIL_HPP
#define UTIL_HPP

#include "infix_ostream_iterator.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <set>
#include <sstream>
#include <string>



const int CHANGESET_MAX_ELEMENTS = 10000;
const int WAY_MAX_NODES = 2000;
const long SCALE = 10000000;

const std::string MAX_TIME_OPEN = "1 day";
const std::string IDLE_TIMEOUT = "1 hour";



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

	for (unsigned int i = 0; i < input.length(); i++)
	{
		if (input.at(i) == '\"')
			result += "\\";
		else if (input.at(i) == '\\')
			result += "\\";

		result += input.at(i);
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

	bbox_t() : minlat(200 * SCALE), minlon(200 * SCALE), maxlat(-200 * SCALE), maxlon(-200 * SCALE) {};

	bbox_t(double _minlat, double _minlon, double _maxlat, double _maxlon) :
	  minlat(_minlat * SCALE), minlon(_minlon * SCALE), maxlat(_maxlat * SCALE), maxlon(_maxlon * SCALE) {};

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


