#ifndef API06_ID_VERSION_IO_HPP
#define API06_ID_VERSION_IO_HPP

#include "cgimap/api06/id_version.hpp"
#include <iostream>

namespace api06 {

std::ostream &operator<<(std::ostream &out, const api06::id_version &idv);

} // namespace api06

#endif /* API06_ID_VERSION_IO_HPP */
