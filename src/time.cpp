#include "cgimap/time.hpp"
#include <boost/date_time/time_parsing.hpp>
#include <stdexcept>
#include <sstream>
#include <ctype.h>

namespace bpt = boost::posix_time;
namespace bdt = boost::date_time;

bpt::ptime parse_time(const std::string &s) {
  // parse only YYYY-MM-DDTHH:MM:SSZ
  if ((s.size() == 20) && (s[19] == 'Z')) {
    // chop off the trailing 'Z' so as not to interfere with the standard
    // parsing.
    std::string without_tz = s.substr(0, 19);

    bpt::ptime t = bdt::parse_delimited_time<bpt::ptime>(without_tz, 'T');

    // if parsing succeeded, then return the time parsed. else fall back to
    // printing an error.
    if (!t.is_special()) {
      return t;
    }
  }

  std::ostringstream ostr;
  ostr << "Unable to parse string '" << s << "' as an ISO 8601 format date time.";
  throw std::runtime_error(ostr.str());
}
