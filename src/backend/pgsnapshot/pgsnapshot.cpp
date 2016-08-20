#include "cgimap/backend/pgsnapshot/snapshot_selection.hpp"
#include "cgimap/backend.hpp"

#include <boost/make_shared.hpp>
#include <sstream>

namespace po = boost::program_options;
using boost::shared_ptr;
using std::string;

namespace {
struct pgsnapshot_backend : public backend {
  pgsnapshot_backend()
      : m_name("pgsnapshot"), m_options("pgsnapshot backend options") {
    m_options.add_options()("dbname", po::value<string>(), "database name")(
        "host", po::value<string>(), "database server host")(
        "username", po::value<string>(), "database user name")(
        "password", po::value<string>(), "database password")(
        "charset", po::value<string>()->default_value("utf8"),
        "database character set")(
        "dbport", po::value<string>(),
        "database port number or UNIX socket file name");
  }
  virtual ~pgsnapshot_backend() {}

  const string &name() const { return m_name; }
  const po::options_description &options() const { return m_options; }

  shared_ptr<data_selection::factory> create(const po::variables_map &opts) {
    if (opts.count("dbname") == 0) {
      throw std::runtime_error("database name not specified");
    }
    shared_ptr<data_selection::factory> factory;
    factory = boost::make_shared<snapshot_selection::factory>(opts);

    return factory;
  }

  boost::shared_ptr<oauth::store> create_oauth_store(
    const po::variables_map &opts) {
    return boost::shared_ptr<oauth::store>();
  }

private:
  string m_name;
  po::options_description m_options;
};
}

boost::shared_ptr<backend> make_pgsnapshot_backend() {
  return boost::make_shared<pgsnapshot_backend>();
}
