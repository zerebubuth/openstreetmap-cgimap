/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include <fmt/core.h>
#include <libmemcached/memcached.h>

#include "cgimap/logger.hpp"
#include "cgimap/options.hpp"
#include "cgimap/rate_limiter.hpp"


std::tuple<bool, int> null_rate_limiter::check(const std::string &, bool) {
  return {false, 0};
}

void null_rate_limiter::update(const std::string &, uint32_t, bool) {
}

struct memcached_rate_limiter::state {
  time_t last_update;
  uint32_t bytes_served;
};

memcached_rate_limiter::memcached_rate_limiter(
    const boost::program_options::variables_map &options) {

  if (!options.count("memcache"))
    return;

  if ((ptr = memcached_create(nullptr)) != nullptr) {

    memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
    memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
    // memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);

    const auto server = options["memcache"].as<std::string>();

    memcached_server_st * server_list = memcached_servers_parse(server.c_str());
    memcached_server_push(ptr, server_list);
    memcached_server_list_free(server_list);

    logger::message(fmt::format("memcached rate limiting enabled ({})", server));
  }
}

memcached_rate_limiter::~memcached_rate_limiter() {
  if (ptr)
    memcached_free(ptr);
}

std::tuple<bool, int> memcached_rate_limiter::check(const std::string &key, bool moderator) {

  uint32_t bytes_served = 0;
  state *sp = nullptr;
  size_t length = 0;
  uint32_t flags = 0;
  memcached_return_t error{};

  const auto mc_key = "cgimap:" + key;
  const auto bytes_per_sec = global_settings::get_ratelimiter_ratelimit(moderator);

  if (ptr &&
      (sp = (state *)memcached_get(ptr, mc_key.data(), mc_key.size(), &length,
                                   &flags, &error)) != nullptr) {
    assert(length == sizeof(state));

    const int64_t elapsed = time(nullptr) - sp->last_update;

    if (elapsed * bytes_per_sec < sp->bytes_served) {
      bytes_served = sp->bytes_served - elapsed * bytes_per_sec;
    }

    free(sp);
  }

  const auto max_bytes = global_settings::get_ratelimiter_maxdebt(moderator);
  if (bytes_served < max_bytes) {
    return {false, 0};
  } else {
    // + 1 to reverse effect of integer flooring seconds
    return {true, (bytes_served - max_bytes) / bytes_per_sec + 1};
  }
}

void memcached_rate_limiter::update(const std::string &key, uint32_t bytes, bool moderator) {

  if (!ptr)
    return;

  const auto now = time(nullptr);

  state *sp = nullptr;
  size_t length = 0;
  uint32_t flags = 0;
  memcached_return_t error{};

  const auto mc_key = "cgimap:" + key;
  const auto bytes_per_sec = global_settings::get_ratelimiter_ratelimit(moderator);

  // upper limit in memcached for relative TTL values
  // anything bigger is considered as absolute timestamp
  constexpr auto REALTIME_MAXDELTA = 60L*60L*24L*30L;

  // calculate number of seconds after which the memcached entry is guaranteed
  // to be irrelevant (adding a bit of headroom).
  const auto relevant_bytes = std::max(global_settings::get_ratelimiter_maxdebt(moderator), bytes);
  const auto memcached_expiration = std::min(REALTIME_MAXDELTA, 2L * relevant_bytes / bytes_per_sec);

retry:

  if (!ptr)
    return;

  if ((sp = (state *)memcached_get(ptr, mc_key.data(), mc_key.size(), &length,
                                   &flags, &error)) != nullptr) {
    assert(length == sizeof(state));

    const int64_t elapsed = now - sp->last_update;

    sp->last_update = now;

    if (elapsed * bytes_per_sec < sp->bytes_served) {
      sp->bytes_served = sp->bytes_served - elapsed * bytes_per_sec + bytes;
    } else {
      sp->bytes_served = bytes;
    }

    // should use CAS but it's a right pain so we'll wing it for now...
    auto rc = memcached_replace(ptr, mc_key.data(), mc_key.size(), (char *)sp,
                      sizeof(state), memcached_expiration, 0);
    free(sp);

  } else {
    state s{};

    s.last_update = now;
    s.bytes_served = bytes;

    auto rc = memcached_add(ptr, mc_key.data(), mc_key.size(), (char *)&s, sizeof(state), memcached_expiration, 0);

    if (rc == MEMCACHED_NOTSTORED) {
      goto retry;
    }
  }
}
