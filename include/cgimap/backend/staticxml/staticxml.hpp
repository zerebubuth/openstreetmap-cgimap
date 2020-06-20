#ifndef STATICXML_BACKEND_HPP
#define STATICXML_BACKEND_HPP

#include "cgimap/backend.hpp"

#include <memory>

std::shared_ptr<backend> make_staticxml_backend();

#endif /* STATICXML_BACKEND_HPP */
