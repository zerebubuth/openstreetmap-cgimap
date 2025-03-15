/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/apidb.hpp"
#include "test_database.hpp"

#include <fstream>
#include <ctime>
#include <sys/time.h>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {

std::string random_db_name() {

  // try to make something that has a reasonable chance of being
  // unique on this machine, in case we clash with anything else.
  auto hash = (unsigned int)getpid();
  struct timeval tv{};
  gettimeofday(&tv, nullptr);
  hash ^= (unsigned int)((tv.tv_usec & 0xffffu) << 16);

  return fmt::format("osm_test_{:08x}", hash);
}

// reads a file of SQL statements, splits on ';' and tries to
// execute them in a transaction.
std::string read_file_contents(const std::filesystem::path& filename) {

  std::streamsize size = fs::file_size(filename);
  std::string query(size, '\0');
  std::ifstream in(filename);
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

    w.exec(fmt::format("CREATE DATABASE {} ENCODING 'UTF8' TEMPLATE template0", db_name));
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
void test_database::setup(const std::filesystem::path& sql_file) {
  pqxx::connection conn(fmt::format("dbname={}", m_db_name));
  setup_schema(conn, sql_file);

  std::shared_ptr<backend> apidb = make_apidb_backend();

  {
    po::options_description desc = apidb->options();
    const char *argv[] = { "", "--dbname", m_db_name.c_str() };
    int argc = sizeof(argv) / sizeof(*argv);
    po::store(po::parse_command_line(argc, argv, desc), vm);
    vm.notify();
    m_readonly_factory = apidb->create(vm);
    m_update_factory = apidb->create_data_update(vm);
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


void test_database::testcase_starting() {

  pqxx::connection conn(fmt::format("dbname={}", m_db_name));
  truncate_all_tables(conn);
}

void test_database::testcase_ended() {

  txn_owner_readonly.reset();
  txn_owner_readwrite.reset();
}

test_database::setup_error::setup_error(std::string str)
  : m_str(std::move(str)) {
}

const char *test_database::setup_error::what() const noexcept {
  return m_str.c_str();
}


std::shared_ptr<data_selection::factory> test_database::get_data_selection_factory() const {
  return m_readonly_factory;
}

// return a data update factory pointing at the current database
std::shared_ptr<data_update::factory> test_database:: get_data_update_factory() const {
  return m_update_factory;
}

std::unique_ptr<data_update::factory> test_database:: get_new_data_update_factory() {
  return make_apidb_backend()->create_data_update(vm);
}

std::unique_ptr<data_selection> test_database::get_data_selection() {
  txn_owner_readonly.reset();
  txn_owner_readonly = m_readonly_factory->get_default_transaction();
  return (*m_readonly_factory).make_selection(*txn_owner_readonly);
}

std::unique_ptr<data_update> test_database::get_data_update() {
  txn_owner_readwrite.reset();
  txn_owner_readwrite = m_update_factory->get_default_transaction();
  return (*m_update_factory).make_data_update(*txn_owner_readwrite);
}

int test_database::run_sql(const std::string &sql) {
  pqxx::connection conn(fmt::format("dbname={}", m_db_name));
  return exec_sql_string(conn, sql);
}

void test_database::setup_schema(pqxx::connection &conn, const std::filesystem::path& sql_file) {
  exec_sql_string(conn, read_file_contents(sql_file));
}
