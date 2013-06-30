#include "backend/apidb/apidb.hpp"
#include "backend/apidb/writeable_pgsql_selection.hpp"
#include "backend/apidb/readonly_pgsql_selection.hpp"
#include "backend.hpp"

#include <boost/make_shared.hpp>
#include <sstream>

namespace po = boost::program_options;
using boost::shared_ptr;
using std::string;

#define CACHE_SIZE 1000

namespace {
struct apidb_backend : public backend {
  apidb_backend() 
    : m_name("apidb"),
      m_options("ApiDB backend options") {
    m_options.add_options()
      ("dbname", po::value<string>(), "database name")
      ("host", po::value<string>(), "database server host")
      ("username", po::value<string>(), "database user name")
      ("password", po::value<string>(), "database password")
      ("charset", po::value<string>()->default_value("utf8"), "database character set")
      ("port", po::value<int>(), "port number to use")
      ("readonly", "use the database in read-only mode")
      ("cachesize", po::value<size_t>()->default_value(CACHE_SIZE), "maximum size of changeset cache")
      ;
  }
  virtual ~apidb_backend() {}

  const string &name() const { return m_name; }
  const po::options_description &options() const { return m_options; }

  shared_ptr<data_selection::factory> create(const po::variables_map &opts) {
    // database type
    bool db_is_writeable = opts.count("readonly") == 0;

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

bool registered = register_backend(boost::make_shared<apidb_backend>());
}
