#include <iostream>
#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>

#include "cgimap/logger.hpp"

using std::string;
using std::ostream;
using std::ofstream;
using boost::format;
using boost::posix_time::ptime;
using boost::posix_time::to_iso_extended_string;
using boost::posix_time::second_clock;
using boost::shared_ptr;

namespace logger {

static shared_ptr<ostream> stream;
static pid_t pid;

void initialise(const string &filename) {
  stream = shared_ptr<ostream>(
      new ofstream(filename.c_str(), std::ios_base::out | std::ios_base::app));
  pid = getpid();
}

void message(const string &m) {
  if (stream) {
    ptime t(second_clock::local_time());
    *stream << "[" << to_iso_extended_string(t) << " #" << pid << "] " << m
            << std::endl;
  }
}

void message(const format &m) { message(m.str()); }
}
