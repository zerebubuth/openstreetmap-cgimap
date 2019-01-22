#ifndef PROCESS_REQUEST_HPP
#define PROCESS_REQUEST_HPP

#include "cgimap/request.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/data_update.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/basicauth.hpp"
#include "cgimap/oauth.hpp"
#include <string>

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter,
                     const std::string &generator, routes &route,
                     std::shared_ptr<data_selection::factory> factory,
                     std::shared_ptr<data_update::factory> update_factory,
                     std::shared_ptr<oauth::store> store);

// TODO: temporary workaround only for test cases
void process_request(request &req, rate_limiter &limiter,
                     const std::string &generator, routes &route,
                     std::shared_ptr<data_selection::factory> factory,
                     std::shared_ptr<oauth::store> store);

#endif /* PROCESS_REQUEST_HPP */
