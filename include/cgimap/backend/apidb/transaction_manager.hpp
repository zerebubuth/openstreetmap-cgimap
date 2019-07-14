#ifndef TRANSACTION_MANAGER_HPP
#define TRANSACTION_MANAGER_HPP

#include "cgimap/logger.hpp"

#include <chrono>
#include <boost/format.hpp>
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

  pqxx::work m_txn;
};

#endif
