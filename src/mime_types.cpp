/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#if !CMAKE
#include "cgimap/config.hpp"
#endif
#include "cgimap/mime_types.hpp"
#include <stdexcept>

using std::string;
using std::runtime_error;

namespace mime {
string to_string(type t) {
  if (any_type == t) {
    return "*/*";
  } else if (text_plain == t) {
    return "text/plain";
  } else if (application_xml == t) {
    return "application/xml";
#if HAVE_YAJL
  } else if (application_json == t) {
    return "application/json";
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
  } else if (name == "text/plain") {
    t = text_plain;
  } else if (name == "text/xml") {     // alias according to RFC 7303, section 9.2
    t = application_xml;
  } else if (name == "application/xml") {
    t = application_xml;
#if HAVE_YAJL
  } else if (name == "application/json") {
    t = application_json;
#endif
  }

  return t;
}
}
