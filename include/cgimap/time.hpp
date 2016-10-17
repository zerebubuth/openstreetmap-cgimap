#ifndef UTIL_TIME_HPP
#define UTIL_TIME_HPP

#include <boost/date_time/posix_time/posix_time_types.hpp>

// parse a time string (ISO 8601 - YYYY-MM-DDTHH:MM:SSZ)
boost::posix_time::ptime parse_time(const std::string &);

#endif /* UTIL_TIME_HPP */
