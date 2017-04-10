#ifndef OAUTH_HPP
#define OAUTH_HPP

#include <string>
#include <set>
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include "cgimap/request.hpp"
#include "cgimap/types.hpp"

namespace oauth {

/**
 * Interface to an object which can lookup secrets and return the
 * secrets associated with those keys / IDs.
 */
struct secret_store {
  virtual boost::optional<std::string> consumer_secret(const std::string &consumer_key) = 0;
  virtual boost::optional<std::string> token_secret(const std::string &token_id) = 0;
};

/**
 * Interface to an object which can "use" a (nonce, timestamp) tuple, inserting
 * it into a store of such tuples. It will return true if the tuple is unique
 * in the store (i.e: didn't already exist), or false if the tuple already
 * existed. This is used in the OAuth algorithm, as a client/token is not
 * allowed to use a nonce more than once.
 *
 * NOTE: although the wording in the OAuth standard appears to suggest that the
 * (nonce, timestamp, token) tuple is unique, it seems that the implementation
 * simply assumes that nonces are "unique enough" to avoid collisions between
 * different tokens.
 */
struct nonce_store {
  virtual bool use_nonce(const std::string &nonce,
                         uint64_t timestamp) = 0;
};

/**
 * Interface which checks if a given token allows API access. Users can grant
 * tokens without API access, or revoke tokens access to the API.
 */
struct token_store {
  virtual bool allow_read_api(const std::string &token_id) = 0;
  virtual boost::optional<osm_user_id_t> get_user_id_for_token(
    const std::string &token_id) = 0;
  virtual std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t) = 0;
};

/**
 * Interface which combines all the above, for convenience.
 */
struct store
  : public secret_store, public nonce_store, public token_store {
  virtual ~store();
};

namespace validity {

// signature is present, nonce has not been used before, everything looks
// correct and valid. this returns the oauth token string to be used by
// the rest of the code (e.g: to find the user).
struct copacetic {
  explicit copacetic(const std::string &t) : token(t) {}
  std::string token;
};

// signature is not present - looks like this request hasn't been signed.
struct not_signed {};

// something bad about the oauth request, but in a way which indicates it has
// not been constructed correctly.
struct bad_request {};

// something bad about the oauth request, and it looks like it's an invalid or
// replayed message, or one without the relevant permissions.
struct unauthorized {
  explicit unauthorized(const std::string &r) : reason(r) {}
  std::string reason;
};

typedef boost::variant<
  copacetic,
  not_signed,
  bad_request,
  unauthorized
  > validity;

} // namespace validity

/**
 * Given a request, checks that the OAuth signature is valid.
 */
validity::validity is_valid_signature(request &, secret_store &, nonce_store &,
                                      token_store &);

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
