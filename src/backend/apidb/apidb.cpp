#include "cgimap/backend/apidb/apidb.hpp"
#include "cgimap/backend/apidb/writeable_pgsql_selection.hpp"
#include "cgimap/backend/apidb/readonly_pgsql_selection.hpp"
#include "cgimap/backend.hpp"

#include <boost/make_shared.hpp>
#include <sstream>

namespace po = boost::program_options;
using boost::shared_ptr;
using std::string;

#define CACHE_SIZE 1000

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
      ("readonly", "use the database in read-only mode")
      ("cachesize", po::value<size_t>()->default_value(CACHE_SIZE),
       "maximum size of changeset cache")
      ("dbport", po::value<string>(),
       "database port number or UNIX socket file name");
    // clang-format on
  }
  virtual ~apidb_backend() {}

  const string &name() const { return m_name; }
  const po::options_description &options() const { return m_options; }

  shared_ptr<data_selection::factory> create(const po::variables_map &opts) {
    // database type
    bool db_is_writeable = opts.count("readonly") == 0;

    if (opts.count("dbname") == 0) {
      throw std::runtime_error("database name not specified");
    }

    shared_ptr<data_selection::factory> factory;
    if (db_is_writeable) {
      factory = boost::make_shared<writeable_pgsql_selection::factory>(opts);
    } else {
      factory = boost::make_shared<readonly_pgsql_selection::factory>(opts);
    }

    return factory;
  }

private:
  string m_name;
  po::options_description m_options;
};
}

boost::shared_ptr<backend> make_apidb_backend() {
  return boost::make_shared<apidb_backend>();
}
