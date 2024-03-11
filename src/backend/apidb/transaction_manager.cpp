/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <pqxx/pqxx>


Transaction_Owner_ReadOnly::Transaction_Owner_ReadOnly(pqxx::connection &conn,
    std::set< std::string > &prep_stmt) :
    m_txn { conn }, m_prep_stmt { prep_stmt }
{
}

pqxx::transaction_base& Transaction_Owner_ReadOnly::get_transaction()
{
  return m_txn;
}

std::set<std::string>& Transaction_Owner_ReadOnly::get_prep_stmt()
{
  return m_prep_stmt;
}

Transaction_Owner_ReadWrite::Transaction_Owner_ReadWrite(pqxx::connection &conn,
    std::set< std::string > &prep_stmt) :
    m_txn { conn }, m_prep_stmt { prep_stmt }
{
}

pqxx::transaction_base& Transaction_Owner_ReadWrite::get_transaction()
{
  return m_txn;
}

std::set<std::string>& Transaction_Owner_ReadWrite::get_prep_stmt()
{
  return m_prep_stmt;
}

Transaction_Manager::Transaction_Manager(Transaction_Owner_Base &to) :
    m_txn { to.get_transaction() }, m_prep_stmt(to.get_prep_stmt())
{
}

void Transaction_Manager::prepare(const std::string &name,
                                  const std::string &definition) {
  if (m_prep_stmt.find(name) == m_prep_stmt.end())
  {
    m_txn.conn().prepare(name, definition);
    m_prep_stmt.insert(name);
  }
}

pqxx::result Transaction_Manager::exec(const std::string &query,
                                       const std::string &) {
  return m_txn.exec(query);
}

void Transaction_Manager::commit() { m_txn.commit(); }
