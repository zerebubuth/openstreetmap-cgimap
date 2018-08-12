#ifndef BASICAUTH_HPP
#define BASICAUTH_HPP

#include <string>

class PasswordHash {
 public:
  bool check(const std::string& pass_crypt,
		    const std::string& pass_salt,
                    const std::string& candidate);

 private:
  template <class T>
  std::string PBKDF2_HMAC_SHA_string(const std::string& pass,
                                     const std::string& salt,
                                     const uint iterations,
                                     const uint outputBytes);

  std::string md5_hash(const std::string& s);

  std::string base64decode(const std::string& s);
};

#endif
