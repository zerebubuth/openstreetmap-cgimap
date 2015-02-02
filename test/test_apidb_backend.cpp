#include <iostream>
#include <fstream>
#include <stdexcept>
#include <boost/noncopyable.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <pqxx/pqxx>

#include <sys/time.h>
#include <stdio.h>

#include "cgimap/backend/apidb/apidb.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace {

/**
 * test_database is a RAII object to create a unique apidb format database
 * populated with fake data to allow the apidb data selection process to
 * be tested in isolation.
 */
struct test_database : public boost::noncopyable {
  // simple error type - we distinguish this from a programming error and
  // allow the test to be skipped, as people might not have or want an
  // apidb database set up on their local machines.
  struct setup_error : public std::exception {
    setup_error(const boost::format &fmt) : m_str(fmt.str()) {}
    ~setup_error() throw() {}
    virtual const char *what() const throw() { return m_str.c_str(); }

  private:
    const std::string m_str;
  };

  // set up a unique test database.
  test_database();

  // drop the test database.
  ~test_database();

  // create table structure and fill with fake data.
  void setup();

  // run a test. func will be called once with each of a writeable and
  // readonly data selection backed by the database. the func should
  // do its own testing - the run method here is just plumbing.
  void run(boost::function<void(boost::shared_ptr<data_selection>)> func);

private:
  // create a random, and hopefully unique, database name.
  static std::string random_db_name();

  // set up the schema of the database and fill it with test data.
  static void fill_fake_data(pqxx::connection &w);

  // the name of the test database.
  std::string m_db_name;

  // factories using the test database which produce writeable and
  // read-only data selections.
  boost::shared_ptr<data_selection::factory> m_writeable_factory,
      m_readonly_factory;
};

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
}

/**
 * set up table structure and create access resources. this isn't
 * involved with the table creation per se, so can error independently
 * and cause a rollback / table drop.
 */
void test_database::setup() {
  pqxx::connection conn((boost::format("dbname=%1%") % m_db_name).str());
  fill_fake_data(conn);

  boost::shared_ptr<backend> apidb = make_apidb_backend();

  {
    po::options_description desc = apidb->options();
    const char *argv[] = { "", "--dbname", m_db_name.c_str() };
    int argc = sizeof(argv) / sizeof(*argv);
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    vm.notify();
    m_writeable_factory = apidb->create(vm);
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
    boost::function<void(boost::shared_ptr<data_selection>)> func) {
  try {
    func((*m_writeable_factory).make_selection());
  } catch (const std::exception &e) {
    throw std::runtime_error(
        (boost::format("%1%, in writeable selection") % e.what()).str());
  }

  try {
    func((*m_readonly_factory).make_selection());
  } catch (const std::exception &e) {
    throw std::runtime_error(
        (boost::format("%1%, in read-only selection") % e.what()).str());
  }
}

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

void test_database::fill_fake_data(pqxx::connection &conn) {
  conn.perform(file_read_transactor("test/structure.sql"));
  conn.perform(file_read_transactor("test/test_apidb_backend.sql"));
}

void test_single_nodes(boost::shared_ptr<data_selection> sel) {
  if (sel->check_node_visibility(1) != data_selection::exists) {
    throw std::runtime_error("Node 1 should be visible, but isn't");
  }
  if (sel->check_node_visibility(2) != data_selection::exists) {
    throw std::runtime_error("Node 2 should be visible, but isn't");
  }

  std::list<osm_id_t> ids;
  ids.push_back(1);
  ids.push_back(2);
  if (sel->select_nodes(ids) != 2) {
    throw std::runtime_error("Selecting 2 nodes failed");
  }
  if (sel->select_nodes(ids) != 0) {
    throw std::runtime_error("Re-selecting 2 nodes failed");
  }
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
        &test_single_nodes));

  } catch (const test_database::setup_error &e) {
    std::cout << "Unable to set up test database: " << e.what() << std::endl;
    return 77;

  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cout << "Unknown exception type." << std::endl;
    return 99;
  }

  return 0;
}
