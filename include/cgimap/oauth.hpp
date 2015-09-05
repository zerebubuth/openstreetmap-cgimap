#ifndef OAUTH_HPP
#define OAUTH_HPP

#include <string>
#include <boost/optional.hpp>
#include "cgimap/request.hpp"

namespace oauth {

/**
 * Given a request, checks that the OAuth signature is valid.
 */
bool is_valid_signature(request &);

namespace detail {

/**
 * Given a request, returns the signature base string as defined
 * by the OAuth standard.
 *
 * Returns boost::none if the OAuth Authorization header could
 * not be parsed.
 */
boost::optional<std::string> signature_base_string(request &req);

/**
 * Given a request, returns a string containing the normalised
 * request parameters. See
 * http://oauth.net/core/1.0a/#rfc.section.9.1.1
 *
 * Returns boost::none if the OAuth Authorization header could
 * not be parsed.
 */
boost::optional<std::string> normalise_request_parameters(request &req);

/**
 * Given a request, returns a string representing the normalised
 * URL according to the OAuth standard. See
 * http://oauth.net/core/1.0a/#rfc.section.9.1.2
 */
std::string normalise_request_url(request &req);

} // namespace detail

} // namespace oauth

#endif /* OAUTH_HPP */
