#include "cgimap/time.hpp"

#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <time.h>


void assert_eq_time(std::chrono::system_clock::time_point expected, std::string str) {
  const std::chrono::system_clock::time_point tb = parse_time(str);
  if (expected != tb) {
      std::time_t time_ex  = std::chrono::system_clock::to_time_t(expected);
      std::time_t time_tb  = std::chrono::system_clock::to_time_t(tb);

    std::ostringstream ostr;
    ostr << "Expected " << std::put_time( std::gmtime( &time_ex ), "%FT%T")  << ", but got time "
         << std::put_time( std::gmtime( &time_tb ), "%FT%T") << " [from '" << str << "'] instead.";
    throw std::runtime_error(ostr.str());
  }
}

int main(int argc, char *argv[]) {
  try {
     struct tm dat{};
     dat.tm_year  = 2015 - 1900;
     dat.tm_mon   = 8 - 1;
     dat.tm_mday  = 31;
     dat.tm_hour  = 23;
     dat.tm_min   = 40;
     dat.tm_sec   = 10;

     std::time_t tm_t = timegm(&dat);

    assert_eq_time(std::chrono::system_clock::from_time_t(tm_t), "2015-08-31T23:40:10Z");

  } catch (const std::exception &ex) {
    std::cerr << "EXCEPTION: " << ex.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN ERROR" << std::endl;
    return 1;
  }

  return 0;
}
