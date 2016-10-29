#ifndef TEST_TEST_DATABASE_HPP
#define TEST_TEST_DATABASE_HPP

#include <stdexcept>
#include <string>

#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/format.hpp>

#include <pqxx/pqxx>

#include "cgimap/data_selection.hpp"
#include "cgimap/oauth.hpp"

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
    setup_error(const boost::format &fmt);
    ~setup_error() throw();
    virtual const char *what() const throw();

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

  // run a test. func will be called once with the OAuth store backed by
  // the database.
  void run(boost::function<void(boost::shared_ptr<oauth::store>)> func);

  void run(boost::function<void(test_database&)> func);

  boost::shared_ptr<data_selection> get_data_selection();

  void run_sql(const std::string &sql);

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

  // oauth store based on the writeable connection.
  boost::shared_ptr<oauth::store> m_oauth_store;

  // whether to use read-only code (true) or writeable code (false)
  bool m_use_readonly;
};

#endif /* TEST_TEST_DATABASE_HPP */
