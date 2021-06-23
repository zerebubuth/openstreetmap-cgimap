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

  boost::optional<osm_user_id_t> validate_bearer_token(request &req, std::shared_ptr<oauth::store>& store, bool& allow_api_write)
  {
    const char * auth_hdr = req.get_param ("HTTP_AUTHORIZATION");
    if (auth_hdr == nullptr)
      return boost::none;

    const auto auth_header = std::string(auth_hdr);

    std::smatch sm;

    try {
	std::regex r("Bearer ([A-Za-z0-9~_\\-\\.\\+\\/]+=*)");   // according to RFC 6750, section 2.1

	if (!std::regex_match(auth_header, sm, r))
	  return boost::none;

	if (sm.size() != 2)
	  return boost::none;

    } catch (std::regex_error&) {
      return boost::none;
    }

    const auto bearer_token_hashed = sha256_hash(sm[1]);

    bool expired;
    bool revoked;
    const auto user_id = store->get_user_id_for_oauth2_token(bearer_token_hashed, expired, revoked, allow_api_write);

    if (boost::none == user_id) {
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
