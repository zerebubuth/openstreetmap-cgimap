#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <string>
#include <libmemcached/memcached.h>
#include <boost/program_options.hpp>

struct rate_limiter {
  virtual ~rate_limiter();

  // check if the key is below the rate limit. return true to indicate that it
  // is.
  virtual std::tuple<bool, int> check(const std::string &key, bool moderator) = 0;

  // update the limit for the key to say it has consumed this number of bytes.
  virtual void update(const std::string &key, int bytes, bool moderator) = 0;
};

struct null_rate_limiter
  : public rate_limiter {
  ~null_rate_limiter();
  std::tuple<bool, int> check(const std::string &key, bool moderator);
  void update(const std::string &key, int bytes, bool moderator);
};

class memcached_rate_limiter
  : public rate_limiter {
public:
  /**
   * Methods.
   */
  memcached_rate_limiter(const boost::program_options::variables_map &options);
  ~memcached_rate_limiter();
  std::tuple<bool, int> check(const std::string &key, bool moderator);
  void update(const std::string &key, int bytes, bool moderator);

private:
  memcached_st *ptr;

  struct state;
};

#endif
