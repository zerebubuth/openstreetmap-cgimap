#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "cgimap/logger.hpp"

using std::string;
using std::ostream;
using std::ofstream;
using boost::format;

using std::shared_ptr;

namespace logger {

static shared_ptr<ostream> stream;
static pid_t pid;

void initialise(const string &filename) {
  stream = std::shared_ptr<ostream>(
      new ofstream(filename.c_str(), std::ios_base::out | std::ios_base::app));
  pid = getpid();
}

void message(const string &m) {
  if (stream) {
    time_t now = time(0);
    *stream << "[" << std::put_time( std::gmtime( &now ), "%FT%T") << " #" << pid << "] " << m
            << std::endl;
  }
}

void message(const format &m) { message(m.str()); }
}
