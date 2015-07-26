#ifndef OAUTH_HPP
#define OAUTH_HPP

#include <string>
#include "cgimap/request.hpp"

/**
 * Given a request, checks that the OAuth signature is valid.
 */
bool oauth_valid_signature(request &);

#endif /* OAUTH_HPP */
