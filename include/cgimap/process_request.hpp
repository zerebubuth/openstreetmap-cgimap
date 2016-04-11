#ifndef PROCESS_REQUEST_HPP
#define PROCESS_REQUEST_HPP

#include "cgimap/request.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/oauth.hpp"
#include <string>
#include <boost/shared_ptr.hpp>

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter,
                     const std::string &generator, routes &route,
                     boost::shared_ptr<data_selection::factory> factory,
                     boost::shared_ptr<oauth::store> store);

#endif /* PROCESS_REQUEST_HPP */
