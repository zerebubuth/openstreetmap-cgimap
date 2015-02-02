#ifndef ROUTES_HPP
#define ROUTES_HPP

#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>
#include "cgimap/config.hpp"

// internal implementation of the routes
struct router;

/**
 * encapsulates routing (URL to handler mapping) information, similar in
 * intent, if not form, to rails' routes.rb.
 */
class routes : public boost::noncopyable {
public:
  routes();
  ~routes();

  /**
         * returns the handler which matches a request, or throws a 404 error.
         */
  handler_ptr_t operator()(request &req) const;

private:
  // common prefix of all routes
  std::string common_prefix;

  // object which actually does the routing.
  boost::scoped_ptr<router> r;

#ifdef ENABLE_API07
  // common prefix of API 0.7 routes.
  std::string experimental_prefix;

  // and an API 0.7 router object
  boost::scoped_ptr<router> r_experimental;
#endif /* ENABLE_API07 */
};

#endif /* ROUTES_HPP */
