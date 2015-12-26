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

/**
 * Interface to an object which can lookup secrets and return the
 * secrets associated with those keys / IDs.
 */
struct secret_store {
  virtual boost::optional<std::string> consumer_secret(const std::string &consumer_key) = 0;
  virtual boost::optional<std::string> token_secret(const std::string &token_id) = 0;
};

namespace detail {

/**
 * Returns the hashed signature of the request, or none if that
 * can't be completed.
 */
boost::optional<std::string> hashed_signature(request &req, secret_store &store);

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

/**
 * Given a string, returns the base64 encoded version of that string,
 * without inserting any linebreaks.
 */
std::string base64_encode(const std::string &str);

/**
 * Given a string key and text, return the SHA-1 hashed message
 * authentication code.
 *
 * Note: Can throw an exception.
 */
std::string hmac_sha1(const std::string &key, const std::string &text);

} // namespace detail

} // namespace oauth

#endif /* OAUTH_HPP */
