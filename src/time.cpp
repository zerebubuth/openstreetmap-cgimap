#include "cgimap/time.hpp"

#include <stdexcept>
#include <sstream>
#include <cctype>
#include <time.h>
#include <fmt/core.h>



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

  throw std::runtime_error(fmt::format("Unable to parse string '{}' as an ISO 8601 format date time.", s));
}
