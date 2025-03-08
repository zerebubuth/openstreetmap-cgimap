/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TRANSACTION_MANAGER_HPP
#define TRANSACTION_MANAGER_HPP

#include "cgimap/logger.hpp"

#include <chrono>
#include <set>
#include <string_view>
#include <string>
#include <utility>

#include <fmt/core.h>
#include <pqxx/pqxx>


#if PQXX_VERSION_MAJOR >= 7

class Stream_Wrapper
{
public:
  Stream_Wrapper(pqxx::transaction_base &txn, std::string_view table,
      std::string_view columns) :

      m_stream(pqxx::stream_to::raw_table(txn, table, columns)), m_table(table), m_start(
          std::chrono::steady_clock::now())
  {
  }

  ~Stream_Wrapper()
  {
    if (m_stream)
      m_stream.complete();
  }

  template< typename ... Ts > void write_values(Ts const &...fields)
  {
    m_stream.write_values(std::forward<Ts const& >(fields)...);
    row_count++;
  }

  void complete()
  {
    m_stream.complete();
    log_stats();
  }

  int row_count = 0;

private:

  void log_stats()
  {
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast < std::chrono::milliseconds > (end - m_start);

    logger::message(fmt::format(
            "Executed COPY statement for table {} in {:d} ms, inserted {:d} rows",
            m_table, elapsed.count(), row_count));
  }

  pqxx::stream_to m_stream;
  const std::string_view m_table;
  const std::chrono::steady_clock::time_point m_start;

};
#endif

class Transaction_Owner_Base
{
public:
  Transaction_Owner_Base() = default;
  virtual pqxx::transaction_base& get_transaction() = 0;
  virtual std::set<std::string>& get_prep_stmt() = 0;
  virtual ~Transaction_Owner_Base() = default;
};


class Transaction_Owner_ReadOnly : public Transaction_Owner_Base
{
public:
  explicit Transaction_Owner_ReadOnly(pqxx::connection &conn, std::set<std::string> &prep_stmt);
  pqxx::transaction_base& get_transaction() override;
  std::set<std::string>& get_prep_stmt() override;
  ~Transaction_Owner_ReadOnly() override = default;

private:
  pqxx::read_transaction m_txn;
  std::set<std::string>& m_prep_stmt;
};


class Transaction_Owner_ReadWrite : public Transaction_Owner_Base
{
public:
  explicit Transaction_Owner_ReadWrite(pqxx::connection &conn, std::set<std::string> &prep_stmt);
  pqxx::transaction_base& get_transaction() override;
  std::set<std::string>& get_prep_stmt() override;
  ~Transaction_Owner_ReadWrite() override = default;

private:
  pqxx::work m_txn;
  std::set<std::string>& m_prep_stmt;
};

class Transaction_Owner_Void : public Transaction_Owner_Base
{
public:
  explicit Transaction_Owner_Void() = default;
  inline pqxx::transaction_base& get_transaction() override {
    throw std::runtime_error ("get_transaction is not supported by Transaction_Owner_Void");
  }

  inline std::set<std::string>& get_prep_stmt() override {
    throw std::runtime_error ("get_prep_stmt is not supported by Transaction_Owner_Void");
  }

  ~Transaction_Owner_Void() override = default;
};


#define PQXX_LIBRARY_VERSION_COMPARE(major1, minor1, patch1, major2, minor2, patch2)   \
    ((major1) < (major2)) ||                                              \
    ((major1) == (major2) && (minor1) < (minor2)) ||                      \
    ((major1) == (major2) && (minor1) == (minor2) && (patch1) < (patch2))


class Transaction_Manager {

public:
  explicit Transaction_Manager(Transaction_Owner_Base &to);

  void prepare(const std::string &name, const std::string &);

  pqxx::result exec(const std::string &query,
                    const std::string &description = std::string());

  void commit() {
    const auto start = std::chrono::steady_clock::now();
    m_txn.commit();
    const auto end = std::chrono::steady_clock::now();

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    logger::message(fmt::format("COMMIT transaction in {:d} ms", elapsed.count()));
  }

  template<typename... Args>
  [[nodiscard]] pqxx::result exec_prepared(const std::string &statement, Args&&... args) {

    const auto start = std::chrono::steady_clock::now();

#if PQXX_LIBRARY_VERSION_COMPARE(PQXX_VERSION_MAJOR, PQXX_VERSION_MINOR, PQXX_VERSION_PATCH, 7, 9, 3)
    auto res(m_txn.exec_prepared(statement, std::forward<Args>(args)...));
#else
    auto res(m_txn.exec(pqxx::prepped{statement}, pqxx::params{std::forward<Args>(args)...}));
#endif
    const auto end = std::chrono::steady_clock::now();

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    logger::message(fmt::format("Executed prepared statement {} in {:d} ms, returning {:d} rows, {:d} affected rows",
                               statement,
                               elapsed.count(),
			       res.size(),
			       res.affected_rows()));
    return res;
  }

#if PQXX_VERSION_MAJOR >= 7
  Stream_Wrapper to_stream(std::string_view table, std::string_view columns) {
    return Stream_Wrapper(m_txn, table, columns);
  }
#endif

private:
  pqxx::transaction_base & m_txn;
  std::set<std::string>& m_prep_stmt;
};

#undef PQXX_LIBRARY_VERSION_COMPARE

#endif
