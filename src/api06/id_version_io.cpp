#include "cgimap/api06/id_version.hpp"
#include <iostream>

namespace api06 {

std::ostream &operator<<(std::ostream &out, const api06::id_version &idv) {
  out << idv.id;
  if (idv.version) {
    out << "v" << idv.version.get();
  }
  return out;
}

} // namespace api06
