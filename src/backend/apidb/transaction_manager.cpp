#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <pqxx/pqxx>

Transaction_Manager::Transaction_Manager(pqxx::connection &conn) : m_txn{ conn } {}

void Transaction_Manager::prepare(const std::string &name,
                                  const std::string &definition) {
  m_txn.conn().prepare(name, definition);
}

pqxx::prepare::invocation
Transaction_Manager::prepared(const std::string &statement) {
  return m_txn.prepared(statement);
}

pqxx::result Transaction_Manager::exec(const std::string &query,
                                       const std::string &description) {
  return m_txn.exec(query, description);
}

void Transaction_Manager::commit() { m_txn.commit(); }

