#include "cgimap/config.hpp"
#include "cgimap/mime_types.hpp"
#include <stdexcept>

using std::string;
using std::runtime_error;

namespace mime {
string to_string(type t) {
  if (any_type == t) {
    return "*/*";
  } else if (text_xml == t) {
    return "text/xml";
#ifdef HAVE_YAJL
  } else if (text_json == t) {
    return "text/json";
#endif
  } else {
    throw runtime_error("No string conversion for unspecified MIME type.");
  }
}

type parse_from(const std::string &name) {
  type t = unspecified_type;

  if (name == "*") {
    t = any_type;
  } else if (name == "*/*") {
    t = any_type;
  } else if (name == "text/*") {
    t = any_type;
  } else if (name == "text/xml") {
    t = text_xml;
#ifdef HAVE_YAJL
  } else if (name == "text/json") {
    t = text_json;
#endif
  }

  return t;
}
}
