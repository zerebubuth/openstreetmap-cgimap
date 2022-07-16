#ifndef APIDB_BACKEND_HPP
#define APIDB_BACKEND_HPP

#include "cgimap/backend.hpp"

#include <memory>

std::unique_ptr<backend> make_apidb_backend();

#endif /* APIDB_BACKEND_HPP */
