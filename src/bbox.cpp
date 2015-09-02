#include "cgimap/bbox.hpp"
#include <cmath>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace al = boost::algorithm;
using boost::lexical_cast;
using boost::bad_lexical_cast;
using std::string;
using std::max;
using std::min;
using std::vector;

bbox::bbox(double minlat_, double minlon_, double maxlat_, double maxlon_)
    : minlat(minlat_), minlon(minlon_), maxlat(maxlat_), maxlon(maxlon_) {}

bbox::bbox() : minlat(0.0), minlon(0.0), maxlat(0.0), maxlon(0.0) {}

bool bbox::operator==(const bbox &other) const {
  return ((minlat == other.minlat) &&
          (minlon == other.minlon) &&
          (maxlat == other.maxlat) &&
          (maxlon == other.maxlon));
}

bool bbox::parse(const std::string &s) {
  vector<string> strs;
  al::split(strs, s, al::is_any_of(","));

  if (strs.size() != 4)
    return false;

  try {
    bbox b(lexical_cast<double>(strs[1]), lexical_cast<double>(strs[0]),
           lexical_cast<double>(strs[3]), lexical_cast<double>(strs[2]));

    // update the current object.
    *this = b;

  } catch (const bad_lexical_cast &) {
    return false;
  }

  return true;
}

void bbox::clip_to_world() {
  minlon = max(minlon, -180.0);
  minlat = max(minlat, -90.0);
  maxlon = min(maxlon, 180.0);
  maxlat = min(maxlat, 90.0);
}

bool bbox::valid() const {
  bool is_valid = true;
  is_valid &= (minlon >= -180.0) && (minlon <= 180.0);
  is_valid &= (maxlon >= -180.0) && (maxlon <= 180.0);
  is_valid &= (minlat >= -90.0) && (minlat <= 90.0);
  is_valid &= (maxlat >= -90.0) && (maxlat <= 90.0);
  is_valid &= minlon <= maxlon;
  is_valid &= minlat <= maxlat;
  return is_valid;
}

double bbox::area() const { return (maxlon - minlon) * (maxlat - minlat); }
