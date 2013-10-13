#ifndef PROCESS_REQUEST_HPP
#define PROCESS_REQUEST_HPP

#include "request.hpp"
#include "rate_limiter.hpp"
#include "data_selection.hpp"
#include "routes.hpp"
#include <string>
#include <boost/shared_ptr.hpp>

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter, const std::string &generator, routes &route,
                     boost::shared_ptr<data_selection::factory> factory);

#endif /* PROCESS_REQUEST_HPP */
