#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
#include "cgimap/backend/apidb/pgsql_update.hpp"
#include "cgimap/backend/apidb/oauth_store.hpp"
#include "cgimap/backend.hpp"

#include <memory>
#include <sstream>

namespace po = boost::program_options;
using std::shared_ptr;
using std::string;

#define CACHE_SIZE 100000

namespace {
struct apidb_backend : public backend {
  apidb_backend() : m_name("apidb"), m_options("ApiDB backend options") {
    // clang-format off
    m_options.add_options()
      ("dbname", po::value<string>(), "database name")
      ("host", po::value<string>(), "database server host")
      ("username", po::value<string>(), "database user name")
      ("password", po::value<string>(), "database password")
      ("charset", po::value<string>()->default_value("utf8"),
       "database character set")
      ("readonly", "(obsolete parameter, read only backend is always assumed)")
      ("disable-api-write", "disable API write operations")
      ("cachesize", po::value<size_t>()->default_value(CACHE_SIZE),
       "maximum size of changeset cache")
      ("dbport", po::value<string>(),
       "database port number or UNIX socket file name")
      ("oauth-dbname", po::value<string>(),
       "database name to use for OAuth, if different from --dbname")
      ("oauth-host", po::value<string>(),
       "database server host for OAuth, if different from --host")
      ("oauth-username", po::value<string>(),
       "database user name for OAuth, if different from --username")
      ("oauth-password", po::value<string>(),
       "database password for OAuth, if different from --password")
      ("oauth-charset", po::value<string>(),
       "database character set for OAuth, if different from --charset")
      ("oauth-dbport", po::value<string>(),
       "database port for OAuth, if different from --dbport")
      ("update-dbname", po::value<string>(),
       "database name to use for API write operations, if different from --dbname")
      ("update-host", po::value<string>(),
       "database server host for API write operations, if different from --host")
      ("update-username", po::value<string>(),
       "database user name for API write operations, if different from --username")
      ("update-password", po::value<string>(),
       "database password for API write operations, if different from --password")
      ("update-charset", po::value<string>(),
       "database character set for API write operations, if different from --charset")
      ("update-dbport", po::value<string>(),
       "database port for API write operations, if different from --dbport");
    // clang-format on
  }
  virtual ~apidb_backend() = default;

  const string &name() const { return m_name; }
  const po::options_description &options() const { return m_options; }

  shared_ptr<data_selection::factory> create(const po::variables_map &opts) {

    if (opts.count("dbname") == 0) {
      throw std::runtime_error("database name not specified");
    }

    return std::make_shared<readonly_pgsql_selection::factory>(opts);
  }

  shared_ptr<data_update::factory> create_data_update(const po::variables_map &opts) {

    if (opts.count("dbname") == 0) {
      throw std::runtime_error("database name not specified");
    }

    return std::make_shared<pgsql_update::factory>(opts);
  }


  std::shared_ptr<oauth::store> create_oauth_store(
    const po::variables_map &opts) {
    shared_ptr<oauth::store> store =
      std::make_shared<oauth_store>(opts);
    return store;
  }

private:
  string m_name;
  po::options_description m_options;
};
}

std::shared_ptr<backend> make_apidb_backend() {
  return std::make_shared<apidb_backend>();
}
