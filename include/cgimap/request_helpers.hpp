/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef REQUEST_HELPERS_HPP
#define REQUEST_HELPERS_HPP

#include "cgimap/request.hpp"
#include "cgimap/http.hpp"

#include <string>
#include <memory>

/**
 * Lookup a string from the request environment. Throws 500 error if the
 * string isn't there and no default value is given.
 */
std::string fcgi_get_env(const request &req, 
                         const char *name,
                         const char *default_value = nullptr);

/**
 * get a query string by hook or by crook.
 *
 * the $QUERY_STRING variable is supposed to be set, but it isn't if
 * cgimap is invoked on the 404 path, which seems to be a pretty common
 * case for doing routing/queueing in lighttpd. in that case, try and
 * parse the $REQUEST_URI.
 */
std::string get_query_string(const request &req);

/**
 * get the path from the $REQUEST_URI variable.
 */
std::string get_request_path(const request &req);

/**
 * get encoding to use for response.
 */
std::unique_ptr<http::encoding> get_encoding(const request &req);

/**
 * return shared pointer to a buffer object which can be
 * used to write to the response body.
 */
std::unique_ptr<output_buffer> make_output_buffer(const request &req);

#endif /* REQUEST_HELPERS_HPP */
