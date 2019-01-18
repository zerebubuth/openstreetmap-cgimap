#ifndef TRANSACTION_MANAGER_HPP
#define TRANSACTION_MANAGER_HPP

#include <pqxx/pqxx>

class Transaction_Manager {

public:
  explicit Transaction_Manager(pqxx::connection &conn);

  void prepare(const std::string &name, const std::string &);

  pqxx::prepare::invocation
  prepared(const std::string &statement = std::string());

  pqxx::result exec(const std::string &query,
                    const std::string &description = std::string());

  void commit();

private:
  pqxx::work m_txn;
};

#endif
