#ifndef OAUTH_IO_HPP
#define OAUTH_IO_HPP

#include "cgimap/oauth.hpp"

namespace oauth {
namespace validity {

inline bool operator==(const copacetic &a, const copacetic &b) {
  return a.token == b.token;
}

inline bool operator==(const not_signed &, const not_signed &) { return true; }
inline bool operator==(const bad_request &, const bad_request &) { return true; }
inline bool operator==(const unauthorized &, const unauthorized &) { return true; }

inline bool operator!=(const copacetic &a, const copacetic &b) { return !(a == b); }
inline bool operator!=(const not_signed &a, const not_signed &b) { return !(a == b); }
inline bool operator!=(const bad_request &a, const bad_request &b) { return !(a == b); }
inline bool operator!=(const unauthorized &a, const unauthorized &b) { return !(a == b); }

std::ostream &operator<<(std::ostream &out, const copacetic &c) {
  out << "copacetic(" << c.token << ")";
  return out;
}

std::ostream &operator<<(std::ostream &out, const not_signed &) {
  return out << "not_signed";
}

std::ostream &operator<<(std::ostream &out, const bad_request &) {
  return out << "bad_request";
}

std::ostream &operator<<(std::ostream &out, const unauthorized &) {
  return out << "unauthorized";
}

} // namespace validity
} // namespace oauth

#endif /* OAUTH_IO_HPP */
