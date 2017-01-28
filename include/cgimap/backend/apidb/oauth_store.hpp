#ifndef APIDB_OAUTH_STORE_HPP
#define APIDB_OAUTH_STORE_HPP

#include "cgimap/oauth.hpp"
#include <pqxx/pqxx>
#include <boost/program_options.hpp>

/**
 */
class oauth_store : public oauth::store {
public:
  oauth_store(const boost::program_options::variables_map &);
  virtual ~oauth_store();

  boost::optional<std::string> consumer_secret(const std::string &consumer_key);
  boost::optional<std::string> token_secret(const std::string &token_id);
  bool use_nonce(const std::string &nonce, uint64_t timestamp);
  bool allow_read_api(const std::string &token_id);
  boost::optional<osm_user_id_t> get_user_id_for_token(const std::string &token_id);
  std::set<osm_user_role_t> get_roles_for_user(osm_user_id_t id);

private:
  pqxx::connection m_connection;
#if PQXX_VERSION_MAJOR >= 4
  pqxx::quiet_errorhandler m_errorhandler;
#endif
};

#endif /* APIDB_OAUTH_STORE_HPP */
