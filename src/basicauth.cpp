#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <cryptopp/base64.h>
#include <cryptopp/config.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/md5.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/secblock.h>
#include <cryptopp/sha.h>
#include <sys/types.h>
#include <argon2.h>
#include <cassert>
#include <iostream>
#include <regex>
#include <string>

#include <boost/algorithm/string.hpp>


#include "cgimap/basicauth.hpp"
#include "cgimap/options.hpp"


using namespace CryptoPP;

bool PasswordHash::check(const std::string& pass_crypt,
			 const std::string& pass_salt,
			 const std::string& candidate) {

  std::string hashed_candidate;

  if (pass_salt.empty()) {

      if (is_valid_argon2(pass_crypt, candidate))
	return true;

      const auto digest = md5_hash(candidate);
      return (boost::iequals(digest, pass_crypt));
  }

  std::vector<std::string> params;  // fields: algorithm, iterations, salt
  boost::split(params, pass_salt, boost::is_any_of("!"));

  if (params.size() != 3) {
      const auto digest = md5_hash(pass_salt + candidate);
      return (boost::iequals(digest, pass_crypt));
  }

  const auto algorithm = params[0];
  const auto iterations = stoi(params[1]);
  const auto salt = params[2];

  const auto pass_crypt_decoded = base64decode(pass_crypt);

  if (algorithm == "sha512")
    hashed_candidate = PBKDF2_HMAC_SHA_string<SHA512>(
	candidate, salt, iterations, pass_crypt_decoded.size());
  else
    throw std::runtime_error("Unknown hash algorithm");

  return (hashed_candidate == pass_crypt);  // both strings are base64 encoded
}


template <class T>
std::string PasswordHash::PBKDF2_HMAC_SHA_string(const std::string& pass,
							const std::string& salt,
							const uint iterations,
							const uint outputBytes) {

  SecByteBlock result(outputBytes);
  std::string base64Result;

  PKCS5_PBKDF2_HMAC<T> pbkdf;

  pbkdf.DeriveKey(result, result.size(), 0x00, reinterpret_cast<const byte*>(pass.data()),
		  pass.size(), reinterpret_cast<const byte*>(salt.data()), salt.size(), iterations);

  ArraySource resultEncoder(
      result, result.size(), true,
      new Base64Encoder(new StringSink(base64Result), false));

  return base64Result;
}

std::string PasswordHash::md5_hash(const std::string& s) {

  Weak::MD5 hash;
  std::string digest;
  StringSource ss(
      s, true,
      new HashFilter(hash, new HexEncoder(new StringSink(digest), false)));
  return digest;
}

std::string PasswordHash::base64decode(const std::string& s) {

  std::string result;
  StringSource ss(s, true, new Base64Decoder(new StringSink(result)));
  return result;
}

bool PasswordHash::is_valid_argon2(const std::string& pass_crypt, const std::string& candidate)
{
  // Argon2 public APIs don't support a secret (pepper) at this time without re-implementing parts of argon2,
  // hence using argon2_verify without secret for the time being.
  // Upstream issue: https://github.com/P-H-C/phc-winner-argon2/issues/314
  // Also assuming id algorithm as discussed in https://github.com/openstreetmap/openstreetmap-website/pull/3353

  return (argon2_verify(pass_crypt.c_str(), candidate.c_str(), candidate.size(), Argon2_id) == ARGON2_OK);
}


namespace basicauth {

  [[nodiscard]] std::optional<osm_user_id_t> authenticate_user(const request &req, data_selection& selection)
  {

    // Basic auth disabled in global configuration settings?
    if (!(global_settings::get_basic_auth_support()))
      return {};

    std::string user_name;
    std::string candidate;

    osm_user_id_t user_id;
    std::string pass_crypt;
    std::string pass_salt;

    const char * auth_hdr = req.get_param ("HTTP_AUTHORIZATION");
    if (auth_hdr == nullptr)
      return {};

    auto auth_header = std::string(auth_hdr);

    std::smatch sm;

    try {
	std::regex r("[Bb][Aa][Ss][Ii][Cc] ([A-Za-z0-9\\+\\/]+=*).*");

	if (!std::regex_match(auth_header, sm, r))
	  return {};

	if (sm.size() != 2)
	  return {};

    } catch (std::regex_error&) {
      return {};
    }

    std::string auth;

    try {
       auth = PasswordHash::base64decode(sm[1]);
    } catch (...) {
      return {};
    }

    auto pos = auth.find(":");

    if (pos == std::string::npos)
      return {};

    try {
      user_name = auth.substr(0, pos);
      candidate = auth.substr(pos + 1);
    } catch (std::out_of_range&) {
       return {};
    }

    if (user_name.empty() || candidate.empty())
      return {};

    auto user_exists = selection.get_user_id_pass(user_name, user_id, pass_crypt, pass_salt);

    if (!user_exists)
      throw http::unauthorized("Incorrect user or password");

    if (PasswordHash::check(pass_crypt, pass_salt, candidate))
      return std::optional<osm_user_id_t>{user_id};
    else
      throw http::unauthorized("Incorrect user or password");

  }
}
