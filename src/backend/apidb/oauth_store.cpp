#include "cgimap/backend/apidb/oauth_store.hpp"
#include "cgimap/logger.hpp"

#if PQXX_VERSION_MAJOR >= 4
#define PREPARE_ARGS(args)
#else
#define PREPARE_ARGS(args) args
#endif

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
#if PQXX_VERSION_MAJOR >= 4
  , m_errorhandler(m_connection)
#endif
{

  // set the connections to use the appropriate charset.
  std::string db_charset = opts["charset"].as<std::string>();
  if (opts.count("oauth-charset")) {
    db_charset = opts["oauth-charset"].as<std::string>();
  }
  m_connection.set_client_encoding(db_charset);

  // ignore notice messages
#if PQXX_VERSION_MAJOR < 4
  m_connection.set_noticer(
      std::auto_ptr<pqxx::noticer>(new pqxx::nonnoticer()));
#endif

  logger::message("Preparing OAuth prepared statements.");

  // clang-format off

  // insert a new nonce into the table, returning whether there was an existing
  // row.
  m_connection.prepare("use_nonce",
    "INSERT INTO oauth_nonces(nonce, \"timestamp\") "
      "SELECT $1::varchar, $2::integer "
      "WHERE NOT EXISTS ("
        "SELECT 1 FROM oauth_nonces "
        "WHERE nonce=$1 AND \"timestamp\"=$2)")
    PREPARE_ARGS(("character varying")("integer"));

  // return a row if there's a token with the given ID which is authorized and
  // valid.
  m_connection.prepare("token_is_valid",
    "SELECT 1 FROM oauth_tokens "
    "WHERE "
      "token=$1 AND "
      "authorized_at IS NOT NULL AND "
      "invalidated_at IS NULL")
    PREPARE_ARGS(("character varying"));

  // return a row with the user ID of the owner of the given token ID.
  m_connection.prepare("token_belongs_to",
    "SELECT user_id FROM oauth_tokens "
    "WHERE token=$1")
    PREPARE_ARGS(("character varying"));

  // return a row with the consumer secret for a given consumer key.
  m_connection.prepare("consumer_secret_for_key",
    "SELECT secret FROM client_applications "
    "WHERE key=$1")
    PREPARE_ARGS(("character varying"));

  // return a row with the token secret given the token ID.
  m_connection.prepare("token_secret_for_id",
    "SELECT secret FROM oauth_tokens "
    "WHERE token=$1")
    PREPARE_ARGS(("character varying"));

  // return all the roles to which the user belongs.
  m_connection.prepare("roles_for_user",
    "SELECT role FROM user_roles WHERE user_id = $1")
    PREPARE_ARGS(("bigint"));

  // clang-format on
}

oauth_store::~oauth_store() {}

boost::optional<std::string>
oauth_store::consumer_secret(const std::string &consumer_key) {
  pqxx::work w(m_connection, "oauth_get_consumer_secret_for_key");
  pqxx::result res = w.prepared("consumer_secret_for_key")(consumer_key).exec();

  if (res.affected_rows() > 0) {
    return res[0][0].as<std::string>();

  } else {
    return boost::none;
  }
}

boost::optional<std::string>
oauth_store::token_secret(const std::string &token_id) {
  pqxx::work w(m_connection, "oauth_get_token_secret_for_id");
  pqxx::result res = w.prepared("token_secret_for_id")(token_id).exec();

  if (res.affected_rows() > 0) {
    return res[0][0].as<std::string>();

  } else {
    return boost::none;
  }
}

bool
oauth_store::use_nonce(const std::string &nonce, uint64_t timestamp) {
  pqxx::work w(m_connection, "oauth_use_nonce");
  pqxx::result res = w.prepared("use_nonce")(nonce)(timestamp).exec();
  return res.affected_rows() > 0;
}

bool
oauth_store::allow_read_api(const std::string &token_id) {
  pqxx::work w(m_connection, "oauth_check_allow_read_api");
  pqxx::result res = w.prepared("token_is_valid")(token_id).exec();
  return res.affected_rows() > 0;
}

boost::optional<osm_user_id_t>
oauth_store::get_user_id_for_token(const std::string &token_id) {
  pqxx::work w(m_connection, "oauth_get_user_id_for_token");
  pqxx::result res = w.prepared("token_belongs_to")(token_id).exec();

  if (res.affected_rows() > 0) {
    osm_user_id_t uid = res[0][0].as<osm_user_id_t>();
    return uid;

  } else {
    return boost::none;
  }
}

std::set<osm_user_role_t>
oauth_store::get_roles_for_user(osm_user_id_t id) {
  std::set<osm_user_role_t> roles;

  pqxx::work w(m_connection, "oauth_get_user_id_for_token");
  pqxx::result res = w.prepared("roles_for_user")(id).exec();

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
