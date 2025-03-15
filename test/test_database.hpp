/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEST_TEST_DATABASE_HPP
#define TEST_TEST_DATABASE_HPP


#include <filesystem>
#include <exception>
#include <string>

#include <fmt/core.h>
#include <pqxx/pqxx>

#include "cgimap/data_selection.hpp"
#include "cgimap/data_update.hpp"

#include "cgimap/backend/apidb/transaction_manager.hpp"

/**
 * test_database is a RAII object to create a unique apidb format database
 * populated with fake data to allow the apidb data selection process to
 * be tested in isolation.
 */
struct test_database {
  // simple error type - we distinguish this from a programming error and
  // allow the test to be skipped, as people might not have or want an
  // apidb database set up on their local machines.
  struct setup_error : public std::exception {
    explicit setup_error(std::string fmt);
    ~setup_error() noexcept override = default;
    const char *what() const noexcept override;

  private:
    const std::string m_str;
  };

  test_database(const test_database &) = delete;
  test_database& operator=(const test_database &) = delete;
  test_database(test_database &&) = default;
  test_database& operator=(test_database &&) = default;

  // set up a unique test database.
  test_database();

  // drop the test database.
  ~test_database();

  // create table structure and fill with fake data.
  void setup(const std::filesystem::path& sql_file = "test/structure.sql");

  // return a data selection factory pointing at the current database
  [[nodiscard]] std::shared_ptr<data_selection::factory> get_data_selection_factory() const;

  // return a data update factory pointing at the current database
  [[nodiscard]] std::shared_ptr<data_update::factory> get_data_update_factory() const;

  // return a new data update factory pointing at the current database,
  // with a fresh database connection
  [[nodiscard]] std::unique_ptr<data_update::factory> get_new_data_update_factory();

  // return a data selection pointing at the current database
  [[nodiscard]] std::unique_ptr<data_selection> get_data_selection();

  // return a data updater pointing at the current database
  [[nodiscard]] std::unique_ptr<data_update> get_data_update();

  // run a (possible set of) SQL strings against the database.
  // intended for setting up data that the test needs.
  int run_sql(const std::string &sql);

  // clean up database tables before new test case starts
  void testcase_starting();

  // clean up internal buffers when test case ended
  void testcase_ended();

private:
  // set up the schema of the database
  static void setup_schema(pqxx::connection &w, const std::filesystem::path& sql_file);

  // the name of the test database.
  std::string m_db_name;

  po::variables_map vm;

  // factories using the test database which produce read-only data selections.
  std::shared_ptr<data_selection::factory> m_readonly_factory;

  std::shared_ptr<data_update::factory> m_update_factory;

  std::unique_ptr<Transaction_Owner_Base> txn_owner_readonly;
  std::unique_ptr<Transaction_Owner_Base> txn_owner_readwrite;
};

#endif /* TEST_TEST_DATABASE_HPP */
