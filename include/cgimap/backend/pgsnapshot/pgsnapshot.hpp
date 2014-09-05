#ifndef PGSNAPSHOT_BACKEND_HPP
#define PGSNAPSHOT_BACKEND_HPP

#include <boost/shared_ptr.hpp>
#include "cgimap/backend.hpp"

boost::shared_ptr<backend> make_pgsnapshot_backend();

#endif /* PGSNAPSHOT_BACKEND_HPP */
