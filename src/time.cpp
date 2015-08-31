#include "cgimap/time.hpp"
#include <boost/date_time/time_parsing.hpp>
#include <stdexcept>
#include <sstream>
#include <ctype.h>

boost::posix_time::ptime parse_time(const std::string &s) {
  // parse only YYYY-MM-DDTHH:MM:SSZ
  if ((s.size() == 20) &&
      (isdigit(s[0]) != 0) &&
      (isdigit(s[1]) != 0) &&
      (isdigit(s[2]) != 0) &&
      (isdigit(s[3]) != 0) &&
      (s[4] == '-') &&
      (isdigit(s[5]) != 0) &&
      (isdigit(s[6]) != 0) &&
      (s[7] == '-') &&
      (isdigit(s[8]) != 0) &&
      (isdigit(s[9]) != 0) &&
      (s[10] == 'T') &&
      (isdigit(s[11]) != 0) &&
      (isdigit(s[12]) != 0) &&
      (s[13] == ':') &&
      (isdigit(s[14]) != 0) &&
      (isdigit(s[15]) != 0) &&
      (s[16] == ':') &&
      (isdigit(s[17]) != 0) &&
      (isdigit(s[18]) != 0) &&
      (s[19] == 'Z')) {
    return boost::posix_time::ptime(
      boost::gregorian::date(
        int(s[0] - '0') * 1000 +
        int(s[1] - '0') *  100 +
        int(s[2] - '0') *   10 +
        int(s[3] - '0'),
        int(s[5] - '0') *   10 +
        int(s[6] - '0'),
        int(s[8] - '0') *   10 +
        int(s[9] - '0')),
      boost::posix_time::time_duration(
        int(s[11] - '0') *   10 +
        int(s[12] - '0'),
        int(s[14] - '0') *   10 +
        int(s[15] - '0'),
        int(s[17] - '0') *   10 +
        int(s[18] - '0')));
  }

  std::ostringstream ostr;
  ostr << "Unable to parse string '" << s << "' as an ISO 8601 format date time.";
  throw std::runtime_error(ostr.str());
}

