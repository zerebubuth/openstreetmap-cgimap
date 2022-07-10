#ifndef CGIMAP_BACKEND_APIDB_UTILS_HPP
#define CGIMAP_BACKEND_APIDB_UTILS_HPP

#include <pqxx/pqxx>

/* Converts an array_agg array value into a string of comma-separated values */

inline std::string friendly_name(pqxx::array_parser&& parser)
{
  std::string result;

  auto obj = parser.get_next();
  while (obj.first != pqxx::array_parser::done)
  {
    if (obj.first == pqxx::array_parser::string_value) {
      if (!result.empty())
        result += ",";
      result += obj.second;
    }
    obj = parser.get_next();
  }
  return result;
}

/* Converts an array_agg array value into a vector of strings */
inline std::vector<std::string> psql_array_to_vector(pqxx::array_parser&& parser) {
  std::vector<std::string> result;

  auto obj = parser.get_next();
  while (obj.first != pqxx::array_parser::done)
  {
    if (obj.first == pqxx::array_parser::string_value) {
      result.push_back(obj.second);
    }
    obj = parser.get_next();
  }
  return result;
}

inline std::vector<std::string> psql_array_to_vector(std::string str)
{
  return psql_array_to_vector(pqxx::array_parser(str.c_str()));
}


/* checks that the postgres version is sufficient to run cgimap.
 *
 * some queries (e.g: LATERAL join) and functions (multi-parameter unnest) only
 * became available in later versions of postgresql.
 */
void check_postgres_version(pqxx::connection_base &conn);

#endif /* CGIMAP_BACKEND_APIDB_UTILS_HPP */
