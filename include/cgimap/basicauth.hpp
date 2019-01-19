#ifndef BASICAUTH_HPP
#define BASICAUTH_HPP

#include <memory>
#include <string>

#include "cgimap/data_selection.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"

class PasswordHash {
 public:
  bool check(const std::string& pass_crypt,
		    const std::string& pass_salt,
                    const std::string& candidate);

  std::string base64decode(const std::string& s);

 private:
  template <class T>
  std::string PBKDF2_HMAC_SHA_string(const std::string& pass,
                                     const std::string& salt,
                                     const uint iterations,
                                     const uint outputBytes);

  std::string md5_hash(const std::string& s);
};

namespace basicauth {
  boost::optional<osm_user_id_t> authenticate_user(request &req, std::shared_ptr<data_selection>& selection);

}

#endif
