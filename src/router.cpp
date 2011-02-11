#include "router.hpp"

namespace match {

error::error() 
  : std::runtime_error("error!") {
}

match_string::match_string(const std::string &s) : str(s) {
}

match_string::match_string(const char *s) : str(s) {
}

match_string::match_type
match_string::match(part_iterator &begin, const part_iterator &end) const {
  bool matches = false;
  if (begin != end) {
    std::string bit = *begin;
    matches = bit == str;
    ++begin;
  }
  if (!matches) { throw error(); }
  return match_type();
}

match_int::match_int() {
}

match_int::match_type
match_int::match(part_iterator &begin, const part_iterator &end) const {
  if (begin != end) {
    try {
      std::string bit = *begin;
      int x = boost::lexical_cast<int>(bit);
      ++begin;
      return match_type(x);
    } catch (std::exception &e) {
      throw error();
    }
  }
  throw error();
}

match_name::match_name() {
}

match_name::match_type
match_name::match(part_iterator &begin, const part_iterator &end) const {
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

match_begin::match_begin() {
}

match_begin::match_type
match_begin::match(part_iterator &begin, const part_iterator &end) const {
  return match_type();
}

extern const match_begin root_;
extern const match_int int_;
extern const match_name name_;

}
