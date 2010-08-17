#ifndef ROUTES_HPP
#define ROUTES_HPP

#include "handler.hpp"
#include <fcgiapp.h>

/**
 * returns the handler which matches a request, or throws a 404 error.
 */
handler_ptr_t route_request(FCGX_Request &request);

#endif /* ROUTES_HPP */
