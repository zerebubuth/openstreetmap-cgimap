#ifndef APIDB_BACKEND_HPP
#define APIDB_BACKEND_HPP

#include "cgimap/backend.hpp"

#include <memory>

#define SCALE (10000000)

std::shared_ptr<backend> make_apidb_backend();

#endif /* APIDB_BACKEND_HPP */
