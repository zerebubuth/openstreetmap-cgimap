#ifndef API06_ID_VERSION_HPP
#define API06_ID_VERSION_HPP

#include "cgimap/types.hpp"
#include <boost/optional.hpp>

namespace api06 {

struct id_version {
  id_version() : id(0), version() {}
  explicit id_version(osm_nwr_id_t i) : id(i), version() {}
  id_version(osm_nwr_id_t i, uint32_t v) : id(i), version(v) {}

  inline bool operator==(const id_version &other) const {
    return (id == other.id) && (version == other.version);
  }
  inline bool operator!=(const id_version &other) const {
    return !operator==(other);
  }
  inline bool operator<(const id_version &other) const {
    return (id < other.id) ||
      ((id == other.id) && bool(version) &&
       (!bool(other.version) || (version.get() < other.version.get())));
  }
  inline bool operator<=(const id_version &other) const {
    return operator<(other) || operator==(other);
  }
  inline bool operator>(const id_version &other) const {
    return !operator<=(other);
  }
  inline bool operator>=(const id_version &other) const {
    return !operator<(other);
  }

  osm_nwr_id_t id;
  boost::optional<uint32_t> version;
};

} // namespace api06

#endif /* API06_ID_VERSION_HPP */
