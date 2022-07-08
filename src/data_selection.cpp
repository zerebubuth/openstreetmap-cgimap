#include "cgimap/data_selection.hpp"


std::vector<std::string> psql_array_to_vector(std::string str)
{
  return psql_array_to_vector(pqxx::array_parser(str.c_str()));
}

std::vector<std::string> psql_array_to_vector(pqxx::array_parser&& parser) {
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

