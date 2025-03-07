/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <string>

#include <libmemcached/memcached.h>
#include <boost/program_options.hpp>

struct rate_limiter {
  virtual ~rate_limiter() = default;

  // check if the key is below the rate limit. return true to indicate that it
  // is. The int return values indicates the wait time in seconds
  virtual std::tuple<bool, int> check(const std::string &key, bool moderator) = 0;

  // update the limit for the key to say it has consumed this number of bytes.
  virtual void update(const std::string &key, uint32_t bytes, bool moderator) = 0;
};

struct null_rate_limiter : public rate_limiter {
  ~null_rate_limiter() override = default;
  std::tuple<bool, int> check(const std::string &key, bool moderator) override;
  void update(const std::string &key, uint32_t bytes, bool moderator) override;
};

class memcached_rate_limiter : public rate_limiter {
public:
  /**
   * Methods.
   */
  explicit memcached_rate_limiter(const boost::program_options::variables_map &options);
  ~memcached_rate_limiter() override;
  std::tuple<bool, int> check(const std::string &key, bool moderator) override;
  void update(const std::string &key, uint32_t bytes, bool moderator) override;

private:
  memcached_st *ptr = nullptr;

  struct state;
};

#endif
