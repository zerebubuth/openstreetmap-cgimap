#include "cgimap/backend/apidb/transaction_manager.hpp"
#include "cgimap/http.hpp"
#include "cgimap/logger.hpp"

#include <pqxx/pqxx>


Transaction_Owner_ReadOnly::Transaction_Owner_ReadOnly( pqxx::connection &conn) : m_txn{ conn } {}

pqxx::transaction_base& Transaction_Owner_ReadOnly::get_transaction() { return m_txn; }

Transaction_Owner_ReadWrite::Transaction_Owner_ReadWrite( pqxx::connection &conn) : m_txn{ conn } {}

pqxx::transaction_base& Transaction_Owner_ReadWrite::get_transaction() { return m_txn; }


Transaction_Manager::Transaction_Manager(Transaction_Owner_Base &to) : m_txn{ to.get_transaction() } {}

void Transaction_Manager::prepare(const std::string &name,
                                  const std::string &definition) {
  m_txn.conn().prepare(name, definition);
}

pqxx::result Transaction_Manager::exec(const std::string &query,
                                       const std::string &description) {
  return m_txn.exec(query, description);
}

void Transaction_Manager::commit() { m_txn.commit(); }
