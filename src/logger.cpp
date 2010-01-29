#include <iostream>
#include <boost/date_time.hpp>

#include "logger.hpp"

using std::cout;
using std::endl;

namespace pt = boost::posix_time;

logger::logger(void)
{
  pt::time_facet *output_facet = new pt::time_facet();

  cout.imbue(std::locale(std::locale::classic(), output_facet));
  output_facet->format("%Y-%m-%d %H:%M:%S%F");

  pt::ptime t(pt::second_clock::local_time());

  cout << "[" << t << " #" << getpid() << "] ";
}

logger::~logger(void)
{
  cout << endl;
}
