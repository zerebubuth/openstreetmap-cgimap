#include "cgimap/backend/apidb/oauth_store.hpp"
#include "cgimap/logger.hpp"

namespace po = boost::program_options;

namespace {
std::string connect_db_str(const po::variables_map &options) {
  // build the connection string.
  std::ostringstream ostr;

  if (options.count("dbname") == 0 &&
      options.count("oauth-dbname") == 0) {
    throw std::runtime_error("Must provide either one of --dbname or "
                             "--oauth-dbname to configure database "
                             "name for OAuth connections.");
  }

#define CONNOPT(a,b)                                                 \
  if (options.count("oauth-" a)) {                                   \
    ostr << " " << (b "=") << options["oauth-" a].as<std::string>(); \
  } else if (options.count(a)) {                                     \
    ostr << " " << (b "=") << options[a].as<std::string>();          \
  }

  CONNOPT("dbname", "dbname");
  CONNOPT("host", "host");
  CONNOPT("username", "user");
  CONNOPT("password", "password");
  CONNOPT("dbport", "port");

#undef CONNOPT
  return ostr.str();
}

} // anonymous namespace

oauth_store::oauth_store(const po::variables_map &opts)
  : m_connection(connect_db_str(opts))
  , m_errorhandler(m_connection)
{

  // set the connections to use the appropriate charset.
  std::string db_charset = opts["charset"].as<std::string>();
  if (opts.count("oauth-charset")) {
    db_charset = opts["oauth-charset"].as<std::string>();
  }
  m_connection.set_client_encoding(db_charset);

  logger::message("Preparing OAuth prepared statements.");

  // clang-format off

  // insert a new nonce into the table, returning whether there was an existing
  // row.
  m_connection.prepare("use_nonce",
    "INSERT INTO oauth_nonces(nonce, \"timestamp\") "
      "SELECT $1::varchar, $2::integer "
      "WHERE NOT EXISTS ("
        "SELECT 1 FROM oauth_nonces "
        "WHERE nonce=$1 AND \"timestamp\"=$2)");

  // return a row if there's a token with the given ID which is authorized and
  // valid.
  m_connection.prepare("token_is_valid",
    "SELECT 1 FROM oauth_tokens "
    "WHERE "
      "token=$1 AND "
      "authorized_at IS NOT NULL AND "
      "invalidated_at IS NULL");

  // return a row with the user ID of the owner of the given token ID.
  m_connection.prepare("token_belongs_to",
    "SELECT user_id FROM oauth_tokens "
    "WHERE token=$1");

  // return a row with allow_write_api boolean status of the given token ID
  m_connection.prepare("token_allow_write_api",
    "SELECT allow_write_api FROM oauth_tokens "
    "WHERE token=$1");

  // return a row with the consumer secret for a given consumer key.
  m_connection.prepare("consumer_secret_for_key",
    "SELECT secret FROM client_applications "
    "WHERE key=$1");

  // return a row with the token secret given the token ID.
  m_connection.prepare("token_secret_for_id",
    "SELECT secret FROM oauth_tokens "
    "WHERE token=$1");

  // return all the roles to which the user belongs.
  m_connection.prepare("roles_for_user",
    "SELECT role FROM user_roles WHERE user_id = $1");

  // clang-format on
}

oauth_store::~oauth_store() = default;

std::optional<std::string>
oauth_store::consumer_secret(const std::string &consumer_key) {
  pqxx::work w(m_connection, "oauth_get_consumer_secret_for_key");
  pqxx::result res = w.exec_prepared("consumer_secret_for_key", consumer_key);

  if (res.affected_rows() > 0) {
    return res[0][0].as<std::string>();

  } else {
    return {};
  }
}

std::optional<std::string>
oauth_store::token_secret(const std::string &token_id) {
  pqxx::work w(m_connection, "oauth_get_token_secret_for_id");
  pqxx::result res = w.exec_prepared("token_secret_for_id", token_id);

  if (res.affected_rows() > 0) {
    return res[0][0].as<std::string>();

  } else {
    return {};
  }
}

bool
oauth_store::use_nonce(const std::string &nonce, uint64_t timestamp) {
  pqxx::work w(m_connection, "oauth_use_nonce");
  pqxx::result res = w.exec_prepared("use_nonce", nonce, timestamp);

  return res.affected_rows() > 0;
}

bool
oauth_store::allow_read_api(const std::string &token_id) {
  pqxx::work w(m_connection, "oauth_check_allow_read_api");
  pqxx::result res = w.exec_prepared("token_is_valid", token_id);

  return res.affected_rows() > 0;
}

bool
oauth_store::allow_write_api(const std::string &token_id) {
  pqxx::work w(m_connection, "oauth_check_allow_write_api");
  pqxx::result res = w.exec_prepared("token_allow_write_api", token_id);

  return res.affected_rows() > 0 && res[0][0].as<bool>();
}

std::optional<osm_user_id_t>
oauth_store::get_user_id_for_token(const std::string &token_id) {
  pqxx::work w(m_connection, "oauth_get_user_id_for_token");
  pqxx::result res = w.exec_prepared("token_belongs_to", token_id);

  if (res.affected_rows() > 0) {
    auto uid = res[0][0].as<osm_user_id_t>();
    return uid;

  } else {
    return {};
  }
}

std::set<osm_user_role_t>
oauth_store::get_roles_for_user(osm_user_id_t id) {
  std::set<osm_user_role_t> roles;

  pqxx::work w(m_connection, "oauth_get_user_id_for_token");
  pqxx::result res = w.exec_prepared("roles_for_user", id);

  for (const auto &tuple : res) {
    auto role = tuple[0].as<std::string>();
    if (role == "moderator") {
      roles.insert(osm_user_role_t::moderator);
    } else if (role == "administrator") {
      roles.insert(osm_user_role_t::administrator);
    }
  }

  return roles;
}

std::optional<osm_user_id_t> oauth_store::get_user_id_for_oauth2_token(const std::string &token_id, bool& expired, bool& revoked, bool& allow_api_write) {

  m_connection.prepare("oauth2_access_token",
    R"(SELECT resource_owner_id as user_id,
         CASE WHEN expires_in IS NULL THEN false
              ELSE (created_at + expires_in * interval '1' second)  < now() at time zone 'utc'
         END as expired,
         COALESCE(revoked_at < now() at time zone 'utc', false) as revoked,
         'write_api' = any(string_to_array(scopes, ' ')) as allow_api_write
       FROM oauth_access_tokens
       WHERE token = $1)");

  pqxx::work w(m_connection, "oauth_get_user_id_for_oauth2_token");
  pqxx::result res = w.exec_prepared("oauth2_access_token", token_id);

  if (!res.empty()) {
    auto uid = res[0]["user_id"].as<osm_user_id_t>();
    expired = res[0]["expired"].as<bool>();
    revoked = res[0]["revoked"].as<bool>();
    allow_api_write = res[0]["allow_api_write"].as<bool>();
    return uid;

  } else {
    expired = true;
    revoked = true;
    allow_api_write = false;
    return {};
  }
}
