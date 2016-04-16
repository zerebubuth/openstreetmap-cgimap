#include "cgimap/router.hpp"

namespace match {

error::error() : std::runtime_error("error!") {}

match_string::match_string(const std::string &s) : str(s) {}

match_string::match_string(const char *s) : str(s) {}

match_string::match_type match_string::match(part_iterator &begin,
                                             const part_iterator &end) const {
  bool matches = false;
  if (begin != end) {
    std::string bit = *begin;
    matches = bit == str;
    ++begin;
  }
  if (!matches) {
    throw error();
  }
  return match_type();
}

match_osm_id::match_osm_id() {}

match_osm_id::match_type match_osm_id::match(part_iterator &begin,
                                             const part_iterator &end) const {
  if (begin != end) {
    try {
      std::string bit = *begin;
      // note that osm_nwr_id_t is actually unsigned, so we lose a bit of
      // precision here, but it's OK since IDs are postgres 'bigint' types
      // which are also signed, so element 2^63 is unlikely to exist.
      int64_t x = boost::lexical_cast<int64_t>(bit);
      if (x > 0) {
        ++begin;
        return match_type(x);
      }
    } catch (std::exception &e) {
      throw error();
    }
  }
  throw error();
}

match_name::match_name() {}

match_name::match_type match_name::match(part_iterator &begin,
                                         const part_iterator &end) const {
  if (begin != end) {
    try {
      std::string bit = *begin++;
      return match_type(bit);
    } catch (std::exception &e) {
      throw error();
    }
  }
  throw error();
}

match_begin::match_begin() {}

match_begin::match_type match_begin::match(part_iterator &,
                                           const part_iterator &) const {
  return match_type();
}

extern const match_begin root_;
extern const match_osm_id osm_id_;
extern const match_name name_;
}
