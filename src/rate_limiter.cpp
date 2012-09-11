#include <vector>
#include <libmemcached/memcached.h>

#include "rate_limiter.hpp"

struct rate_limiter::state {
  time_t last_update;
  int bytes_served;
};

rate_limiter::rate_limiter(const boost::program_options::variables_map &options)
{
  if (options.count("memcache") &&
      (ptr = memcached_create(NULL)) != NULL)
  {
    memcached_server_st *server_list;

    memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
    memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
//    memcached_behavior_set(ptr, MEMCACHED_BEHAVIOR_SUPPORT_CAS, 1);

    server_list = memcached_servers_parse(options["memcache"].as<std::string>().c_str());

    memcached_server_push(ptr, server_list);

    memcached_server_list_free(server_list);
  }
  else
  {
    ptr = NULL;
  }

  if (options.count("ratelimit"))
  {
    bytes_per_sec = options["ratelimit"].as<int>();
  }
  else
  {
    bytes_per_sec = 100 * 1024;
  }

  if (options.count("maxdebt"))
  {
    max_bytes = options["maxdebt"].as<int>() * 1024 * 1024;
  }
  else
  {
    max_bytes = 250 * 1024 * 1024;
  }
}

rate_limiter::~rate_limiter(void)
{
  if (ptr)
    memcached_free(ptr);
}

bool rate_limiter::check(const std::string &ip)
{
  int bytes_served = 0;
  std::string key;
  state *sp;
  size_t length;
  uint32_t flags;
  memcached_return error;

  key = "cgimap:" + ip;

  if (ptr &&
      (sp = (state *)memcached_get(ptr, key.data(), key.size(), &length, &flags, &error)) != NULL)
  {
    assert(length == sizeof(state));

    int64_t elapsed = time(NULL) - sp->last_update;

    if (elapsed * bytes_per_sec < sp->bytes_served)
    {
       bytes_served = sp->bytes_served - elapsed * bytes_per_sec;
    }

    free(sp);
  }

  return bytes_served < max_bytes;
}

void rate_limiter::update(const std::string &ip, int bytes)
{
  if (ptr)
  {
    time_t now = time(NULL);
    std::string key;
    state *sp;
    size_t length;
    uint32_t flags;
    memcached_return error;

    key = "cgimap:" + ip;

    retry:

    if (ptr &&
        (sp = (state *)memcached_get(ptr, key.data(), key.size(), &length, &flags, &error)) != NULL)
    {
      assert(length == sizeof(state));

      int64_t elapsed = now - sp->last_update;

      sp->last_update = now;

      if (elapsed * bytes_per_sec < sp->bytes_served)
      {
         sp->bytes_served = sp->bytes_served - elapsed * bytes_per_sec + bytes;
      }
      else
      {
         sp->bytes_served = bytes;
      }

      // should use CAS but it's a right pain so we'll wing it for now...
      memcached_replace(ptr, key.data(), key.size(), (char *)sp, sizeof(state), 0, 0);

      free(sp);
    }
    else
    {
      state s;

      s.last_update = now;
      s.bytes_served = bytes;

      if (memcached_add(ptr, key.data(), key.size(), (char *)&s, sizeof(state), 0, 0) == MEMCACHED_NOTSTORED)
      {
        goto retry;
      }
    }
  }

  return;
}
