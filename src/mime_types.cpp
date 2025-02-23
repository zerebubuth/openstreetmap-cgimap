/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/mime_types.hpp"
#include <stdexcept>


namespace mime {
std::string to_string(type t) {
  if (mime::type::any_type == t) {
    return "*/*";
  } else if (mime::type::text_plain == t) {
    return "text/plain";
  } else if (mime::type::application_xml == t) {
    return "application/xml";
  } else if (mime::type::application_json == t) {
    return "application/json";
  } else {
    throw std::runtime_error("No string conversion for unspecified MIME type.");
  }
}

type parse_from(std::string_view name) {

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
  } else if (name == "application/json") {
    return mime::type::application_json;
  }

  return mime::type::unspecified_type;
}
}
