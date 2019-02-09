#include "cgimap/time.hpp"

#include <stdexcept>
#include <sstream>
#include <cctype>
#include <time.h>



std::chrono::system_clock::time_point parse_time(const std::string &s) {
  // parse only YYYY-MM-DDTHH:MM:SSZ
  if ((s.size() == 20) && (s[19] == 'Z')) {
    // chop off the trailing 'Z' so as not to interfere with the standard
    // parsing.
    std::string without_tz = s.substr(0, 19);

    std::tm tm = {};
    strptime(s.c_str(), "%FT%T%z", &tm);
    auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));

    return tp;
  }

  std::ostringstream ostr;
  ostr << "Unable to parse string '" << s << "' as an ISO 8601 format date time.";
  throw std::runtime_error(ostr.str());
}
