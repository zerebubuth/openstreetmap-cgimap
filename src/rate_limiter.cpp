/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <vector>
#include <libmemcached/memcached.h>

#include "cgimap/options.hpp"
#include "cgimap/rate_limiter.hpp"


std::tuple<bool, int> null_rate_limiter::check(const std::string &, bool) {
  return std::make_tuple(false, 0);
}

void null_rate_limiter::update(const std::string &, int, bool) {
}

struct memcached_rate_limiter::state {
  time_t last_update;
  uint32_t bytes_served;
};

memcached_rate_limiter::memcached_rate_limiter(
    const boost::program_options::variables_map &options) {
  if (options.count("memcache") && (ptr = memcached_create(NULL)) != NULL) {
    memcached_server_st *server_list;

    memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
    memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
    // memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);

    server_list =
        memcached_servers_parse(options["memcache"].as<std::string>().c_str());

    memcached_server_push(ptr, server_list);

    memcached_server_list_free(server_list);
  } else {
    ptr = NULL;
  }
}

memcached_rate_limiter::~memcached_rate_limiter() {
  if (ptr)
    memcached_free(ptr);
}

std::tuple<bool, int> memcached_rate_limiter::check(const std::string &key, bool moderator) {
  uint32_t bytes_served = 0;
  std::string mc_key;
  state *sp;
  size_t length;
  uint32_t flags;
  memcached_return error;

  mc_key = "cgimap:" + key;
  auto bytes_per_sec = global_settings::get_ratelimiter_ratelimit(moderator);

  if (ptr &&
      (sp = (state *)memcached_get(ptr, mc_key.data(), mc_key.size(), &length,
                                   &flags, &error)) != NULL) {
    assert(length == sizeof(state));

    int64_t elapsed = time(NULL) - sp->last_update;

    if (elapsed * bytes_per_sec < sp->bytes_served) {
      bytes_served = sp->bytes_served - elapsed * bytes_per_sec;
    }

    free(sp);
  }

  auto max_bytes = global_settings::get_ratelimiter_maxdebt(moderator);
  if (bytes_served < max_bytes) {
    return std::make_tuple(false, 0);
  } else {
    // + 1 to reverse effect of integer flooring seconds
    return std::make_tuple(true, (bytes_served - max_bytes) / bytes_per_sec + 1);
  }
}

void memcached_rate_limiter::update(const std::string &key, int bytes, bool moderator) {
  if (ptr) {
    time_t now = time(NULL);
    std::string mc_key;
    state *sp;
    size_t length;
    uint32_t flags;
    memcached_return error;

    mc_key = "cgimap:" + key;
    auto bytes_per_sec = global_settings::get_ratelimiter_ratelimit(moderator);

  retry:

    if (ptr &&
        (sp = (state *)memcached_get(ptr, mc_key.data(), mc_key.size(), &length,
                                     &flags, &error)) != NULL) {
      assert(length == sizeof(state));

      int64_t elapsed = now - sp->last_update;

      sp->last_update = now;

      if (elapsed * bytes_per_sec < sp->bytes_served) {
        sp->bytes_served = sp->bytes_served - elapsed * bytes_per_sec + bytes;
      } else {
        sp->bytes_served = bytes;
      }

      // should use CAS but it's a right pain so we'll wing it for now...
      memcached_replace(ptr, mc_key.data(), mc_key.size(), (char *)sp,
                        sizeof(state), 0, 0);

      free(sp);
    } else {
      state s;

      s.last_update = now;
      s.bytes_served = bytes;

      if (memcached_add(ptr, mc_key.data(), mc_key.size(), (char *)&s,
                        sizeof(state), 0, 0) == MEMCACHED_NOTSTORED) {
        goto retry;
      }
    }
  }

  return;
}
