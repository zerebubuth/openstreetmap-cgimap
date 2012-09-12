#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <string>
#include <libmemcached/memcached.h>
#include <boost/program_options.hpp>

class rate_limiter {
public:
  /**
   * Methods.
   */
  rate_limiter(const boost::program_options::variables_map &options);
  ~rate_limiter(void);
  bool check(const std::string &ip);
  void update(const std::string &ip, int bytes);

private:
  memcached_st *ptr;
  int bytes_per_sec;
  int max_bytes;

  struct state;
};

#endif
