/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/mime_types.hpp"
#include <stdexcept>

using std::string;
using std::runtime_error;

namespace mime {
string to_string(type t) {
  if (mime::type::any_type == t) {
    return "*/*";
  } else if (mime::type::text_plain == t) {
    return "text/plain";
  } else if (mime::type::application_xml == t) {
    return "application/xml";
#if HAVE_YAJL
  } else if (mime::type::application_json == t) {
    return "application/json";
#endif
  } else {
    throw runtime_error("No string conversion for unspecified MIME type.");
  }
}

type parse_from(const std::string &name) {

  if (name == "*") {
    return mime::type::any_type;
  } else if (name == "*/*") {
    return mime::type::any_type;
  } else if (name == "text/*") {
    return mime::type::any_type;
  } else if (name == "text/plain") {
    return mime::type::text_plain;
  } else if (name == "text/xml") {     // alias according to RFC 7303, section 9.2
    return mime::type::application_xml;
  } else if (name == "application/xml") {
    return mime::type::application_xml;
#if HAVE_YAJL
  } else if (name == "application/json") {
    return mime::type::application_json;
#endif
  }

  return mime::type::unspecified_type;
}
}
