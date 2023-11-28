/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef BASICAUTH_HPP
#define BASICAUTH_HPP

#include <memory>
#include <string>

#include "cgimap/data_selection.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"

class PasswordHash {
 public:
  static bool check(const std::string& pass_crypt,
                    const std::string& pass_salt,
                    const std::string& candidate);

  static std::string base64decode(const std::string& s);

 private:
  template <class T>
  static std::string PBKDF2_HMAC_SHA_string(const std::string& pass,
                                            const std::string& salt,
                                            const uint iterations,
                                            const uint outputBytes);

  static std::string md5_hash(const std::string& s);

  static bool is_valid_argon2(const std::string& pass_crypt, const std::string& candidate);
};

namespace basicauth {
  std::optional<osm_user_id_t> authenticate_user(const request &req, data_selection& selection);

}

#endif
