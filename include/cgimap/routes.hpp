#ifndef ROUTES_HPP
#define ROUTES_HPP

#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"
#include "cgimap/config.hpp"

// internal implementation of the routes
struct router;

/**
 * encapsulates routing (URL to handler mapping) information, similar in
 * intent, if not form, to rails' routes.rb.
 */
class routes {
public:
  routes();
  ~routes();

  routes(const routes &) = delete;
  routes& operator=(const routes &) = delete;
  routes(routes &&) = default;
  routes& operator=(routes &&) = default;

  /**
         * returns the handler which matches a request, or throws a 404 error.
         */
  handler_ptr_t operator()(request &req) const;

private:
  // common prefix of all routes
  std::string common_prefix;

  // object which actually does the routing.
  std::unique_ptr<router> r;

#ifdef ENABLE_API07
  // common prefix of API 0.7 routes.
  std::string experimental_prefix;

  // and an API 0.7 router object
  std::unique_ptr<router> r_experimental;
#endif /* ENABLE_API07 */
};

#endif /* ROUTES_HPP */
