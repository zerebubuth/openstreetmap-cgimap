#ifndef PROCESS_REQUEST_HPP
#define PROCESS_REQUEST_HPP

#include "cgimap/request.hpp"
#include "cgimap/rate_limiter.hpp"
#include "cgimap/data_update.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/basicauth.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/oauth2.hpp"
#include <string>

/**
 * process a single request.
 */
void process_request(request &req, rate_limiter &limiter,
                     const std::string &generator, routes &route,
                     data_selection::factory& factory,
                     data_update::factory* update_factory,
                     oauth::store* store);

#endif /* PROCESS_REQUEST_HPP */
