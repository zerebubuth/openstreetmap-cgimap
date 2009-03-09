#ifndef TEMP_TABLES_HPP
#define TEMP_TABLES_HPP

#include <mysql++/mysql++.h>
#include "bbox.hpp"

#define SCALE (10000000.0)

class tmp_nodes {
public:
  tmp_nodes(mysqlpp::Connection &c, const bbox &bounds);
  ~tmp_nodes();
private:
  mysqlpp::Connection &con;
};

class tmp_ways {
public:
  tmp_ways(mysqlpp::Connection &c);
  ~tmp_ways();
private:
  mysqlpp::Connection &con;
};


#endif /* TEMP_TABLES_HPP */
