#include <iostream>
#include <fstream>
#include <stdexcept>
#include <boost/noncopyable.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional/optional_io.hpp>
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

template <typename T>
void assert_equal(const T& a, const T&b, const std::string &message) {
  if (a != b) {
    throw std::runtime_error(
      (boost::format(
        "Expecting %1% to be equal, but %2% != %3%")
       % message % a % b).str());
  }
}

struct test_formatter : public output_formatter {
  struct node_t {
    node_t(const element_info &elem_, double lon_, double lat_,
           const tags_t &tags_)
      : elem(elem_), lon(lon_), lat(lat_), tags(tags_) {}

    element_info elem;
    double lon, lat;
    tags_t tags;

    inline bool operator!=(const node_t &other) const {
      return !operator==(other);
    }

    bool operator==(const node_t &other) const {
#define CMP(sym) { if ((sym) != other. sym) { return false; } }
      CMP(elem.id);
      CMP(elem.version);
      CMP(elem.changeset);
      CMP(elem.timestamp);
      CMP(elem.uid);
      CMP(elem.display_name);
      CMP(elem.visible);
      CMP(lon);
      CMP(lat);
      CMP(tags.size());
#undef CMP
      return std::equal(tags.begin(), tags.end(), other.tags.begin());
    }
  };

  std::vector<node_t> m_nodes;

  virtual ~test_formatter() {}
  mime::type mime_type() const { throw std::runtime_error("Unimplemented"); }
  void start_document(const std::string &generator) {}
  void end_document() {}
  void write_bounds(const bbox &bounds) {}
  void start_element_type(element_type type) {}
  void end_element_type(element_type type) {}
  void write_node(const element_info &elem, double lon, double lat,
                  const tags_t &tags) {
    m_nodes.push_back(node_t(elem, lon, lat, tags));
  }
  void write_way(const element_info &elem, const nodes_t &nodes,
                 const tags_t &tags) {}
  void write_relation(const element_info &elem,
                      const members_t &members, const tags_t &tags) {}
  void flush() {}

  void error(const std::exception &e) {
    throw e;
  }
  void error(const std::string &str) {
    throw std::runtime_error(str);
  }
};

std::ostream &operator<<(std::ostream &out, const test_formatter::node_t &n) {
  out << "node(element_info("
      << "id=" << n.elem.id << ", "
      << "version=" << n.elem.version << ", "
      << "changeset=" << n.elem.changeset << ", "
      << "timestamp=" << n.elem.timestamp << ", "
      << "uid=" << n.elem.uid << ", "
      << "display_name=" << n.elem.display_name << ", "
      << "visible=" << n.elem.visible << "), "
      << "lon=" << n.lon << ", "
      << "lat=" << n.lat << ", "
      << "{";
  BOOST_FOREACH(const tags_t::value_type &v, n.tags) {
    out << "\"" << v.first << "\" => \"" << v.second << "\", ";
  }
  out << "})";
}

void test_single_nodes(boost::shared_ptr<data_selection> sel) {
  if (sel->check_node_visibility(1) != data_selection::exists) {
    throw std::runtime_error("Node 1 should be visible, but isn't");
  }
  if (sel->check_node_visibility(2) != data_selection::exists) {
    throw std::runtime_error("Node 2 should be visible, but isn't");
  }

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  ids.push_back(2);
  ids.push_back(3);
  ids.push_back(4);
  if (sel->select_nodes(ids) != 4) {
    throw std::runtime_error("Selecting 4 nodes failed");
  }
  if (sel->select_nodes(ids) != 0) {
    throw std::runtime_error("Re-selecting 4 nodes failed");
  }

  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(1), data_selection::exists,
    "node 1 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(2), data_selection::exists,
    "node 2 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(3), data_selection::deleted,
    "node 3 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(4), data_selection::exists,
    "node 4 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(5), data_selection::non_exist,
    "node 5 visibility");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 4, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(2, 1, 1, "2013-11-14T02:10:01Z", 1, std::string("user_1"), true),
      0.1, 0.1,
      tags_t()
      ),
    f.m_nodes[1], "second node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(3, 2, 2, "2015-03-02T18:27:00Z", 1, std::string("user_1"), false),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[2], "third node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(4, 1, 4, "2015-03-02T19:25:00Z", boost::none, boost::none, true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[3], "fourth (anonymous) node written");
}

void test_dup_nodes(boost::shared_ptr<data_selection> sel) {
  if (sel->check_node_visibility(1) != data_selection::exists) {
    throw std::runtime_error("Node 1 should be visible, but isn't");
  }

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(1);
  ids.push_back(1);
  ids.push_back(1);
  if (sel->select_nodes(ids) != 1) {
    throw std::runtime_error("Selecting 3 duplicates of 1 node failed");
  }
  if (sel->select_nodes(ids) != 0) {
    throw std::runtime_error("Re-selecting the same node failed");
  }

  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(1), data_selection::exists,
    "node 1 visibility");

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 1, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(1, 1, 1, "2013-11-14T02:10:00Z", 1, std::string("user_1"), true),
      0.0, 0.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
}

void test_negative_changeset_ids(boost::shared_ptr<data_selection> sel) {
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(6), data_selection::exists,
    "node 6 visibility");
  assert_equal<data_selection::visibility_t>(
    sel->check_node_visibility(7), data_selection::exists,
    "node 7 visibility");

  std::vector<osm_nwr_id_t> ids;
  ids.push_back(6);
  ids.push_back(7);
  if (sel->select_nodes(ids) != 2) {
    throw std::runtime_error("Selecting 2 nodes failed");
  }

  test_formatter f;
  sel->write_nodes(f);
  assert_equal<size_t>(f.m_nodes.size(), 2, "number of nodes written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(6, 1, 0, "2016-04-16T15:09:00Z", boost::none, boost::none, true),
      9.0, 9.0,
      tags_t()
      ),
    f.m_nodes[0], "first node written");
  assert_equal<test_formatter::node_t>(
    test_formatter::node_t(
      element_info(7, 1, -1, "2016-04-16T15:09:00Z", boost::none, boost::none, true),
      9.0, 9.0,
      tags_t()
      ),
    f.m_nodes[1], "second node written");
}

} // anonymous namespace

int main(int, char **) {
  try {
    test_database tdb;
    tdb.setup();

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
        &test_single_nodes));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
        &test_dup_nodes));

    tdb.run(boost::function<void(boost::shared_ptr<data_selection>)>(
        &test_negative_changeset_ids));

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
