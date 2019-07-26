#ifndef TRANSACTION_MANAGER_HPP
#define TRANSACTION_MANAGER_HPP

#include "cgimap/logger.hpp"

#include <chrono>
#include <boost/format.hpp>
#include <pqxx/pqxx>

#include <iostream>


class Transaction_Owner_Base
{
public:
  Transaction_Owner_Base() {}
  virtual pqxx::transaction_base& get_transaction() = 0;
  virtual ~Transaction_Owner_Base() {}
};


class Transaction_Owner_ReadOnly : public Transaction_Owner_Base
{
public:
  explicit Transaction_Owner_ReadOnly(pqxx::connection &conn);
  virtual pqxx::transaction_base& get_transaction();
  ~Transaction_Owner_ReadOnly() {}

private:
  pqxx::read_transaction m_txn;
};


class Transaction_Owner_ReadWrite : public Transaction_Owner_Base
{
public:
  explicit Transaction_Owner_ReadWrite(pqxx::connection &conn);
  virtual pqxx::transaction_base& get_transaction();
  ~Transaction_Owner_ReadWrite() {}

private:
  pqxx::work m_txn;
};

class Transaction_Owner_Void : public Transaction_Owner_Base
{
public:
  explicit Transaction_Owner_Void();
  virtual pqxx::transaction_base& get_transaction();
  ~Transaction_Owner_Void() {}
};



class Transaction_Manager {

public:
  explicit Transaction_Manager(Transaction_Owner_Base &to);

  void prepare(const std::string &name, const std::string &);

  pqxx::prepare::invocation
  prepared(const std::string &statement = std::string());

  pqxx::result exec(const std::string &query,
                    const std::string &description = std::string());
  void commit();

  template<typename... Args>
  pqxx::result exec_prepared(const std::string &statement, Args&&... args) {

    pqxx::prepare::invocation inv = m_txn.prepared(statement);
    pqxx::prepare::invocation inv_with_params = pass_param<Args...>(inv, std::forward<Args>(args)...);

    auto start = std::chrono::steady_clock::now();
    pqxx::result res = inv_with_params.exec();
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    logger::message(boost::format("Executed prepared statement %1% in %2% ms, returning %3% rows, %4% affected rows")
                               % statement
			       % elapsed.count()
			       % res.size()
			       % res.affected_rows());
    return res;
  }

private:

  template<typename Arg>
  pqxx::prepare::invocation pass_param(pqxx::prepare::invocation& in, Arg&& arg)
  {
    return(in(std::forward<Arg>(arg)));
  }

  template<typename Arg, typename... Args>
  pqxx::prepare::invocation pass_param(pqxx::prepare::invocation& in, Arg&& arg, Args&&... args)
  {
    return (pass_param<Args...>(in(std::forward<Arg>(arg)), std::forward<Args>(args)...));
  }

  pqxx::transaction_base & m_txn;
};


#endif
