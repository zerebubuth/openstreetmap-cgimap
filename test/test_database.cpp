#include "cgimap/backend/apidb/apidb.hpp"
#include "test_database.hpp"

#include <fstream>
#include <time.h>
#include <sys/time.h>

#include <filesystem>

namespace fs = std::filesystem;
namespace po = boost::program_options;

test_database::setup_error::setup_error(std::string str)
  : m_str(str) {
}

test_database::setup_error::~setup_error() noexcept = default;

const char *test_database::setup_error::what() const noexcept {
  return m_str.c_str();
}

namespace {

// reads a file of SQL statements, splits on ';' and tries to
// execute them in a transaction.
std::string read_file_contents(const std::string &filename) {

  std::streamsize size = fs::file_size(filename);
  std::string query(size, '\0');
  std::ifstream in(filename.c_str());
  in.read(&query[0], size);
  if (in.fail() || (size != in.gcount())) {
    throw std::runtime_error("Unable to read input SQL file.");
  }
  return query;
}

int exec_sql_string(pqxx::connection& conn, const std::string &s) {

  pqxx::work w{conn};
  int row_count = w.exec(s).size();
  w.commit();
  return row_count;
}



void truncate_all_tables(pqxx::connection& conn) {

  exec_sql_string(conn, "TRUNCATE TABLE users CASCADE");
}

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

    w.exec(fmt::format("CREATE DATABASE {}", db_name));
    w.commit();
    m_db_name = db_name;

  } catch (const std::exception &e) {
    throw setup_error(fmt::format("Unable to set up test database: {}", e.what()));

  } catch (...) {
    throw setup_error("Unable to set up test database due to unknown error.");
  }
}

/**
 * set up table structure and create access resources. this isn't
 * involved with the table creation per se, so can error independently
 * and cause a rollback / table drop.
 */
void test_database::setup() {
  pqxx::connection conn(fmt::format("dbname={}", m_db_name));
  setup_schema(conn);

  std::shared_ptr<backend> apidb = make_apidb_backend();

  {
    po::options_description desc = apidb->options();
    const char *argv[] = { "", "--dbname", m_db_name.c_str() };
    int argc = sizeof(argv) / sizeof(*argv);
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    vm.notify();
    m_readonly_factory = apidb->create(vm);
    m_update_factory = apidb->create_data_update(vm);
    m_oauth_store = apidb->create_oauth_store(vm);
  }
}

test_database::~test_database() {

  if (txn_owner_readonly)
    txn_owner_readonly.reset();

  if (txn_owner_readwrite)
    txn_owner_readwrite.reset();

  if (m_readonly_factory) {
    m_readonly_factory.reset();
  }
  if (m_update_factory) {
      m_update_factory.reset();
  }
  if (m_oauth_store) {
    m_oauth_store.reset();
  }

  if (!m_db_name.empty()) {
    try {
      pqxx::connection conn("dbname=postgres");
      pqxx::nontransaction w(conn);

      w.exec(fmt::format("DROP DATABASE {}", m_db_name));
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
  auto hash = (unsigned int)getpid();
  struct timeval tv;
  gettimeofday(&tv, NULL);
  hash ^= (unsigned int)((tv.tv_usec & 0xffffu) << 16);

  snprintf(name, 20, "osm_test_%08x", hash);
  return std::string(name);
}

void test_database::run(
    std::function<void(test_database&)> func) {

  try {
    // clear out database before using it!
    pqxx::connection conn(fmt::format("dbname={}", m_db_name));
    truncate_all_tables(conn);

    func(*this);

  } catch (const std::exception &e) {
    throw std::runtime_error(fmt::format("%1%", e.what()));
  }
  txn_owner_readonly.reset();
  txn_owner_readwrite.reset();
}

void test_database::run_update(
    std::function<void(test_database&)> func) {

  try {
    // clear out database before using it!
    pqxx::connection conn(fmt::format("dbname={}", m_db_name));
    truncate_all_tables(conn);

    func(*this);
  } catch (const std::exception &e) {
    throw std::runtime_error(fmt::format("{}, in update", e.what()));
  }

  txn_owner_readonly.reset();
  txn_owner_readwrite.reset();
}


std::shared_ptr<data_selection::factory> test_database::get_data_selection_factory() {
  return m_readonly_factory;
}

// return a data update factory pointing at the current database
std::shared_ptr<data_update::factory> test_database:: get_data_update_factory() {
  return m_update_factory;
}


std::shared_ptr<data_selection> test_database::get_data_selection() {
  txn_owner_readonly.reset();
  txn_owner_readonly = m_readonly_factory->get_default_transaction();
  return (*m_readonly_factory).make_selection(*txn_owner_readonly);
}

std::shared_ptr<data_update> test_database::get_data_update() {
  txn_owner_readwrite.reset();
  txn_owner_readwrite = m_update_factory->get_default_transaction();
  return (*m_update_factory).make_data_update(*txn_owner_readwrite);
}

std::shared_ptr<oauth::store> test_database::get_oauth_store() {
  if (!m_oauth_store) {
    throw std::runtime_error("OAuth store not available.");
  }

  return m_oauth_store;
}

int test_database::run_sql(const std::string &sql) {
  pqxx::connection conn(fmt::format("dbname={}", m_db_name));
  return exec_sql_string(conn, sql);
}

void test_database::setup_schema(pqxx::connection &conn) {
  exec_sql_string(conn, read_file_contents("test/structure.sql"));
}
