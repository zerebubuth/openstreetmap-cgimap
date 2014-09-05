#ifndef APIDB_BACKEND_HPP
#define APIDB_BACKEND_HPP

#include <boost/shared_ptr.hpp>
#include "cgimap/backend.hpp"

#define SCALE (10000000)

boost::shared_ptr<backend> make_apidb_backend();

#endif /* APIDB_BACKEND_HPP */
