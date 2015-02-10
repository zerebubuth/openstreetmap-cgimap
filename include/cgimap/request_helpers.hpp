#ifndef REQUEST_HELPERS_HPP
#define REQUEST_HELPERS_HPP

#include <string>
#include <boost/shared_ptr.hpp>
#include "cgimap/request.hpp"
#include "cgimap/http.hpp"

/**
 * Lookup a string from the request environment. Throws 500 error if the
 * string isn't there and no default value is given.
 */
std::string fcgi_get_env(request &req, const char *name,
                         const char *default_value = NULL);

/**
 * get a query string by hook or by crook.
 *
 * the $QUERY_STRING variable is supposed to be set, but it isn't if
 * cgimap is invoked on the 404 path, which seems to be a pretty common
 * case for doing routing/queueing in lighttpd. in that case, try and
 * parse the $REQUEST_URI.
 */
std::string get_query_string(request &req);

/**
 * get the path from the $REQUEST_URI variable.
 */
std::string get_request_path(request &req);

/**
 * get encoding to use for response.
 */
boost::shared_ptr<http::encoding> get_encoding(request &req);

/**
 * return a static string description for an HTTP status code.
 */
const char *status_message(int code);

/**
 * return shared pointer to a buffer object which can be
 * used to write to the response body.
 */
boost::shared_ptr<output_buffer> make_output_buffer(request &req);

#endif /* REQUEST_HELPERS_HPP */
