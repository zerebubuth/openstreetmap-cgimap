#ifndef FCGI_HELPERS_HPP
#define FCGI_HELPERS_HPP

#include <string>
#include <fcgiapp.h>

/**
 * Lookup a string from the FCGI environment. Throws 500 error if the
 * string isn't there.
 */
std::string
fcgi_get_env(FCGX_Request &req, const char* name);

/**
 * get a query string by hook or by crook.
 *
 * the $QUERY_STRING variable is supposed to be set, but it isn't if 
 * cgimap is invoked on the 404 path, which seems to be a pretty common
 * case for doing routing/queueing in lighttpd. in that case, try and
 * parse the $REQUEST_URI.
 */
std::string
get_query_string(FCGX_Request &req);

#endif /* FCGI_HELPERS_HPP */
