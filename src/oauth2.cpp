#include <cryptopp/config.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <sys/types.h>

#include <regex>

#include "cgimap/oauth2.hpp"


inline std::string sha256_hash(const std::string& s) {

  using namespace CryptoPP;

  SHA256 hash;
  std::string digest;
  StringSource ss(
      s, true,
      new HashFilter(hash, new HexEncoder(new StringSink(digest), false)));
  return digest;
}


namespace oauth2 {

  [[nodiscard]] std::optional<osm_user_id_t> validate_bearer_token(const request &req, oauth::store* store, bool& allow_api_write)
  {
    const char * auth_hdr = req.get_param ("HTTP_AUTHORIZATION");
    if (auth_hdr == nullptr)
      return std::nullopt;

    const auto auth_header = std::string(auth_hdr);

    std::smatch sm;

    try {
	std::regex r("Bearer ([A-Za-z0-9~_\\-\\.\\+\\/]+=*)");   // according to RFC 6750, section 2.1

	if (!std::regex_match(auth_header, sm, r))
	  return std::nullopt;

	if (sm.size() != 2)
	  return std::nullopt;

    } catch (std::regex_error&) {
      return std::nullopt;
    }

    const auto bearer_token = sm[1];

    bool expired;
    bool revoked;

    if (!store)
      return std::nullopt;

    // Check token as plain text first
    auto user_id = store->get_user_id_for_oauth2_token(bearer_token, expired, revoked, allow_api_write);

    // Fallback to sha256-hashed token
    if (!(user_id)) {
      const auto bearer_token_hashed = sha256_hash(bearer_token);
      user_id = store->get_user_id_for_oauth2_token(bearer_token_hashed, expired, revoked, allow_api_write);
    }

    if (!(user_id)) {
      throw http::unauthorized("invalid_token");
    }

    if (expired) {
      throw http::unauthorized("token_expired");
    }

    if (revoked) {
      throw http::unauthorized("token_revoked");
    }

    return user_id;
  }
}
