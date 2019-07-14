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
#include <cassert>
#include <iostream>
#include <string>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include "cgimap/basicauth.hpp"


using namespace CryptoPP;

bool PasswordHash::check(const std::string& pass_crypt,
			 const std::string& pass_salt,
			 const std::string& candidate) {

  std::string hashed_candidate;

  if (pass_salt.empty()) {
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

  pbkdf.DeriveKey(result, result.size(), 0x00, (byte*)pass.data(),
		  pass.size(), (byte*)salt.data(), salt.size(), iterations);

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


namespace basicauth {

  boost::optional<osm_user_id_t> authenticate_user(request &req, std::shared_ptr<data_selection>& selection)
  {
    PasswordHash pwd_hash;

    std::string user_name;
    std::string candidate;

    osm_user_id_t user_id;
    std::string pass_crypt;
    std::string pass_salt;

    const char * auth_hdr = req.get_param ("HTTP_AUTHORIZATION");
    if (auth_hdr == nullptr)
      return boost::optional<osm_user_id_t>{};

    auto auth_header = std::string(auth_hdr);

    boost::smatch sm;

    try {
	boost::regex r("[Bb][Aa][Ss][Ii][Cc] ([A-Za-z0-9\\+\\/]+=*).*");

	if (!boost::regex_match(auth_header, sm, r))
	  return boost::optional<osm_user_id_t>{};

	if (sm.size() != 2)
	  return boost::optional<osm_user_id_t>{};

    } catch (boost::regex_error&) {
      return boost::optional<osm_user_id_t>{};
    }

    std::string auth;

    try {
       auth = pwd_hash.base64decode(sm[1]);
    } catch (...) {
      return boost::optional<osm_user_id_t>{};
    }

    auto pos = auth.find(":");

    if (pos == std::string::npos)
      return boost::optional<osm_user_id_t>{};

    try {
      user_name = auth.substr(0, pos);
      candidate = auth.substr(pos + 1);
    } catch (std::out_of_range&) {
       return boost::optional<osm_user_id_t>{};
    }

    if (user_name.empty() || candidate.empty())
      return boost::optional<osm_user_id_t>{};

    auto user_exists = selection->get_user_id_pass(user_name, user_id, pass_crypt, pass_salt);

    if (!user_exists)
      throw http::unauthorized("Incorrect user or password");

    if (pwd_hash.check(pass_crypt, pass_salt, candidate))
      return boost::optional<osm_user_id_t>{user_id};
    else
      throw http::unauthorized("Incorrect user or password");

  }
}
