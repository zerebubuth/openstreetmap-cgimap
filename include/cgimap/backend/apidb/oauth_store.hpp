/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef APIDB_OAUTH_STORE_HPP
#define APIDB_OAUTH_STORE_HPP

#include "cgimap/oauth.hpp"
#include <pqxx/pqxx>
#include <boost/program_options.hpp>

class oauth_store : public oauth::store {
public:
  oauth_store(const boost::program_options::variables_map &);
  ~oauth_store() override;

  std::optional<std::string> consumer_secret(const std::string &consumer_key) override;
  std::optional<std::string> token_secret(const std::string &token_id) override;
  bool use_nonce(const std::string &nonce, uint64_t timestamp) override;
  bool allow_read_api(const std::string &token_id) override;
  bool allow_write_api(const std::string &token_id) override;
  std::optional<osm_user_id_t> get_user_id_for_token(const std::string &token_id) override;
  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t id) override;
  std::optional<osm_user_id_t> get_user_id_for_oauth2_token(const std::string &token_id, bool& expired, bool& revoked, bool& allow_api_write) override;

private:
  pqxx::connection m_connection;
  pqxx::quiet_errorhandler m_errorhandler;
};

#endif /* APIDB_OAUTH_STORE_HPP */
