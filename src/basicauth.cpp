#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <crypto++/base64.h>
#include <crypto++/config.h>
#include <crypto++/filters.h>
#include <crypto++/hex.h>
#include <crypto++/md5.h>
#include <crypto++/pwdbased.h>
#include <crypto++/secblock.h>
#include <crypto++/sha.h>
#include <sys/types.h>
#include <cassert>
#include <iostream>
#include <string>

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


