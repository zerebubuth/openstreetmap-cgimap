#ifndef UTIL_TIME_HPP
#define UTIL_TIME_HPP

#include <string>
#include <chrono>

// parse a time string (ISO 8601 - YYYY-MM-DDTHH:MM:SSZ)
std::chrono::system_clock::time_point parse_time(const std::string &);

#endif /* UTIL_TIME_HPP */
