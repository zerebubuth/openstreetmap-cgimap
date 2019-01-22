#ifndef PGSNAPSHOT_BACKEND_HPP
#define PGSNAPSHOT_BACKEND_HPP

#include "cgimap/backend.hpp"

#include <memory>

std::shared_ptr<backend> make_pgsnapshot_backend();

#endif /* PGSNAPSHOT_BACKEND_HPP */
