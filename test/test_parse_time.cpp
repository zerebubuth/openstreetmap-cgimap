#include "cgimap/time.hpp"
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <stdexcept>
#include <iostream>

namespace pt = boost::posix_time;
namespace gg = boost::gregorian;

void assert_eq_time(pt::ptime expected, std::string str) {
  const pt::ptime tb = parse_time(str);
  if (expected != tb) {
    std::ostringstream ostr;
    ostr << "Expected " << boost::posix_time::to_simple_string(expected) << ", but got time "
         << boost::posix_time::to_simple_string(tb) << " [from '" << str << "'] instead.";
    throw std::runtime_error(ostr.str());
  }
}

int main(int argc, char *argv[]) {
  try {
    assert_eq_time(pt::ptime(gg::date(2015, 8, 31), pt::time_duration(23, 40, 10)),
                   "2015-08-31T23:40:10Z");

  } catch (const std::exception &ex) {
    std::cerr << "EXCEPTION: " << ex.what() << std::endl;
    return 1;

  } catch (...) {
    std::cerr << "UNKNOWN ERROR" << std::endl;
    return 1;
  }

  return 0;
}
