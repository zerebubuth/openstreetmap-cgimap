#include "cgimap/backend/apidb/utils.hpp"

void check_postgres_version(pqxx::connection_base &conn) {
  auto version = conn.server_version();
  if (version < 90400) {
    throw std::runtime_error("Expected Postgres version 9.4+, currently installed version "
      + std::to_string(version));
  }
}
