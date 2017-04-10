#include "cgimap/backend/apidb/apidb.hpp"
#include "test_database.hpp"

#include <fstream>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
namespace po = boost::program_options;

test_database::setup_error::setup_error(const boost::format &fmt)
  : m_str(fmt.str()) {
}

test_database::setup_error::~setup_error() throw() {}

const char *test_database::setup_error::what() const throw() {
  return m_str.c_str();
}

namespace {
// reads a file of SQL statements, splits on ';' and tries to
// execute them in a transaction.
struct file_read_transactor : public pqxx::transactor<pqxx::work> {
  file_read_transactor(const std::string &filename) : m_filename(filename) {}

  void operator()(pqxx::work &w) {
    std::streamsize size = fs::file_size(m_filename);
    std::string big_query(size, '\0');
    std::ifstream in(m_filename.c_str());
    in.read(&big_query[0], size);
    if (in.fail() || (size != in.gcount())) {
      throw std::runtime_error("Unable to read input SQL file.");
    }
    w.exec(big_query);
  }

  std::string m_filename;
};

struct truncate_all_tables : public pqxx::transactor<pqxx::work> {
  void operator()(pqxx::work &w) {
    w.exec("TRUNCATE TABLE users CASCADE");
  }
};

struct sql_string : public pqxx::transactor<pqxx::work> {
  std::string m_sql;
  sql_string(const std::string &s) : m_sql(s) {}

  void operator()(pqxx::work &w) {
    w.exec(m_sql);
  }
};

} // anonymous namespace

/**
 * set up a test database, isolated from anything else, and fill it with
 * fake data for testing.
 */
test_database::test_database() {
  try {
    std::string db_name = random_db_name();

    pqxx::connection conn("dbname=postgres");
    pqxx::nontransaction w(conn);

    w.exec((boost::format("CREATE DATABASE %1%") % db_name).str());
    w.commit();
    m_db_name = db_name;

  } catch (const std::exception &e) {
    throw setup_error(boost::format("Unable to set up test database: %1%") %
                      e.what());

  } catch (...) {
    throw setup_error(
        boost::format("Unable to set up test database due to unknown error."));
  }

  m_use_readonly = false;
}

/**
 * set up table structure and create access resources. this isn't
 * involved with the table creation per se, so can error independently
 * and cause a rollback / table drop.
 */
void test_database::setup() {
  pqxx::connection conn((boost::format("dbname=%1%") % m_db_name).str());
  setup_schema(conn);

  boost::shared_ptr<backend> apidb = make_apidb_backend();

  {
    po::options_description desc = apidb->options();
    const char *argv[] = { "", "--dbname", m_db_name.c_str() };
    int argc = sizeof(argv) / sizeof(*argv);
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    vm.notify();
    m_writeable_factory = apidb->create(vm);
    m_oauth_store = apidb->create_oauth_store(vm);
  }

  {
    po::options_description desc = apidb->options();
    const char *argv[] = { "", "--dbname", m_db_name.c_str(), "--readonly" };
    int argc = sizeof(argv) / sizeof(*argv);
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    vm.notify();
    m_readonly_factory = apidb->create(vm);
  }
}

test_database::~test_database() {
  if (m_writeable_factory) {
    m_writeable_factory.reset();
  }
  if (m_readonly_factory) {
    m_readonly_factory.reset();
  }
  if (m_oauth_store) {
    m_oauth_store.reset();
  }

  if (!m_db_name.empty()) {
    try {
      pqxx::connection conn("dbname=postgres");
      pqxx::nontransaction w(conn);

      w.exec((boost::format("DROP DATABASE %1%") % m_db_name).str());
      w.commit();
      m_db_name.clear();

    } catch (const std::exception &e) {
      // nothing we can do here in the destructor except complain
      // loudly.
      std::cerr << "Unable to drop database: " << e.what() << std::endl;

    } catch (...) {
      std::cerr << "Unable to drop database due to unknown exception."
                << std::endl;
    }
  }
}

std::string test_database::random_db_name() {
  char name[20];

  // try to make something that has a reasonable chance of being
  // unique on this machine, in case we clash with anything else.
  unsigned int hash = (unsigned int)getpid();
  struct timeval tv;
  gettimeofday(&tv, NULL);
  hash ^= (unsigned int)((tv.tv_usec & 0xffffu) << 16);

  snprintf(name, 20, "osm_test_%08x", hash);
  return std::string(name);
}

void test_database::run(
    boost::function<void(test_database&)> func) {
  try {
    // clear out database before using it!
    pqxx::connection conn((boost::format("dbname=%1%") % m_db_name).str());
    conn.perform(truncate_all_tables());

    m_use_readonly = false;
    func(*this);

  } catch (const std::exception &e) {
    throw std::runtime_error(
        (boost::format("%1%, in writeable selection") % e.what()).str());
  }

  try {
    // clear out database before using it!
    pqxx::connection conn((boost::format("dbname=%1%") % m_db_name).str());
    conn.perform(truncate_all_tables());

    m_use_readonly = true;
    func(*this);

  } catch (const std::exception &e) {
    throw std::runtime_error(
        (boost::format("%1%, in read-only selection") % e.what()).str());
  }
}

boost::shared_ptr<data_selection> test_database::get_data_selection() {
  if (m_use_readonly) {
    return (*m_readonly_factory).make_selection();

  } else {
    return (*m_writeable_factory).make_selection();
  }
}

boost::shared_ptr<oauth::store> test_database::get_oauth_store() {
  if (!m_oauth_store) {
    throw std::runtime_error("OAuth store not available.");
  }

  return m_oauth_store;
}

void test_database::run_sql(const std::string &sql) {
  pqxx::connection conn((boost::format("dbname=%1%") % m_db_name).str());
  conn.perform(sql_string(sql));
}

void test_database::setup_schema(pqxx::connection &conn) {
  conn.perform(file_read_transactor("test/structure.sql"));
}
