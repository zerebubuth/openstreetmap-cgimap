#include <vector>
#include <libmemcached/memcached.h>

#include "cgimap/rate_limiter.hpp"

rate_limiter::~rate_limiter() = default;

null_rate_limiter::~null_rate_limiter() = default;

bool null_rate_limiter::check(const std::string &, bool) {
  return true;
}

void null_rate_limiter::update(const std::string &, int, bool) {
}

struct memcached_rate_limiter::state {
  time_t last_update;
  int bytes_served;
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

  if (options.count("ratelimit")) {
    bytes_per_sec = options["ratelimit"].as<int>();
  } else {
    bytes_per_sec = 100 * 1024; // 100 KB/s
  }

  if (options.count("maxdebt")) {
    max_bytes = options["maxdebt"].as<int>() * 1024 * 1024;
  } else {
    max_bytes = 250 * 1024 * 1024; // 250 MB
  }

  if (options.count("moderator-ratelimit")) {
    moderator_bytes_per_sec = options["moderator-ratelimit"].as<int>();
  } else {
    moderator_bytes_per_sec = 1024 * 1024; // 1 MB/s
  }

  if (options.count("moderator-maxdebt")) {
    moderator_max_bytes = options["moderator-maxdebt"].as<int>() * 1024 * 1024;
  } else {
    moderator_max_bytes = 1024 * 1024 * 1024; // 1 GB
  }
}

memcached_rate_limiter::~memcached_rate_limiter() {
  if (ptr)
    memcached_free(ptr);
}

bool memcached_rate_limiter::check(const std::string &key, bool moderator) {
  int bytes_served = 0;
  std::string mc_key;
  state *sp;
  size_t length;
  uint32_t flags;
  memcached_return error;

  mc_key = "cgimap:" + key;
  int specific_bytes_per_sec = moderator ? moderator_bytes_per_sec : bytes_per_sec;

  if (ptr &&
      (sp = (state *)memcached_get(ptr, mc_key.data(), mc_key.size(), &length,
                                   &flags, &error)) != NULL) {
    assert(length == sizeof(state));

    int64_t elapsed = time(NULL) - sp->last_update;

    if (elapsed * specific_bytes_per_sec < sp->bytes_served) {
      bytes_served = sp->bytes_served - elapsed * specific_bytes_per_sec;
    }

    free(sp);
  }

  if (moderator) {
      return bytes_served < moderator_max_bytes;
  }

  return bytes_served < max_bytes;
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
    int specific_bytes_per_sec = moderator ? moderator_bytes_per_sec : bytes_per_sec;

  retry:

    if (ptr &&
        (sp = (state *)memcached_get(ptr, mc_key.data(), mc_key.size(), &length,
                                     &flags, &error)) != NULL) {
      assert(length == sizeof(state));

      int64_t elapsed = now - sp->last_update;

      sp->last_update = now;

      if (elapsed * specific_bytes_per_sec < sp->bytes_served) {
        sp->bytes_served = sp->bytes_served - elapsed * specific_bytes_per_sec + bytes;
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
