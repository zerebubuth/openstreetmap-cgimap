#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
 #include <unistd.h>

#include "cgimap/logger.hpp"
#include <fmt/core.h>

using std::string;
using std::ostream;
using std::ofstream;



namespace logger {

static std::unique_ptr<ostream> stream;
static pid_t pid;

void initialise(const string &filename) {
  stream = std::make_unique<ofstream>(filename.c_str(), std::ios_base::out | std::ios_base::app);
  pid = getpid();
}

void message(const string &m) {
  if (stream) {
    time_t now = time(0);
    *stream << "[" << std::put_time( std::gmtime( &now ), "%FT%T") << " #" << pid << "] " << m
            << std::endl;
  }
}

// void message(const fmt::format &m) { message(m.str()); }

}
