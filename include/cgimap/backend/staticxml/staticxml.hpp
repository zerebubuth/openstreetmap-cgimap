#ifndef STATICXML_BACKEND_HPP
#define STATICXML_BACKEND_HPP

#include <boost/shared_ptr.hpp>
#include "cgimap/backend.hpp"

#define SCALE (10000000)

boost::shared_ptr<backend> make_staticxml_backend();

#endif /* STATICXML_BACKEND_HPP */
