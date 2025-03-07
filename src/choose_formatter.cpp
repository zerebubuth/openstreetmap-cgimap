/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/choose_formatter.hpp"
#include "cgimap/mime_types.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/xml_writer.hpp"
#include "cgimap/xml_formatter.hpp"
#include "cgimap/json_writer.hpp"
#include "cgimap/json_formatter.hpp"
#include "cgimap/text_writer.hpp"
#include "cgimap/text_formatter.hpp"
#include "cgimap/util.hpp"

#include <stdexcept>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <charconv>

#include <fmt/core.h>

namespace {

bool parseQValue(std::string_view param, double &qValue) {
#if !defined(__APPLE__)
  auto [ptr, ec] = std::from_chars(param.begin(), param.end(), qValue,
                                   std::chars_format::fixed);
  return (ec == std::errc() && ptr == param.end());
#else
  std::string p(param);
  for (char ch : p) {
    if (!(std::isdigit(ch) || ch == '.')) {
      return false;  // Invalid character found
    }
  }
  size_t pos;
  try {
    qValue = std::stod(p, &pos);
    return (pos == p.size()); // Check if the entire string was consumed
  } catch (const std::exception &) {
    return false; // Invalid value
  }
#endif
}

} // namespace

AcceptHeader::AcceptHeader(std::string_view header) {

  acceptedTypes = parse(header);

  std::ranges::sort(acceptedTypes,
            [](const AcceptElement &a, const AcceptElement &b) {
              return std::tie(a.q, a.type, a.subtype) >
                     std::tie(b.q, b.type, b.subtype);
            });

  for (const auto &acceptedType : acceptedTypes)
    mapping[acceptedType.mimeType] = acceptedType.q;
}

[[nodiscard]] bool AcceptHeader::is_acceptable(mime::type mt) const {
  return mapping.contains(mt) || mapping.contains(mime::type::any_type);
}

[[nodiscard]] mime::type AcceptHeader::most_acceptable_of(
    const std::vector<mime::type> &available) const {
  mime::type best = mime::type::unspecified_type;
  double score = -1;

  // first, try for an exact match
  for (const auto &type : available) {
    auto itr = mapping.find(type);
    if ((itr != mapping.end()) && (itr->second > score)) {
      best = type;
      score = itr->second;
    }
  }

  // if no exact match found, check for wildcard match
  if (best == mime::type::unspecified_type && !available.empty()) {
    auto itr = mapping.find(mime::type::any_type);
    if (itr != mapping.end()) {
      best = available.front();
      score = itr->second;
    }
  }

  return best;
}

// Parse the accept header and return a vector containing all the
// information about the Accepted types
std::vector<AcceptHeader::AcceptElement> AcceptHeader::parse(std::string_view data) const {

  std::vector<AcceptHeader::AcceptElement> acceptElements;

  // Split by comma to get individual media types
  auto items = split_trim(data, ',');

  if (items.empty())
    throw http::bad_request("Invalid empty accept header");

  for (const auto &item : items) {
    // Split each item by semicolon to separate media type from
    // parameters
    auto elems = split_trim(item, ';');

    if (elems.empty()) {
      throw http::bad_request("Malformed accept header");
    }

    // Treat Accept: * as Accept: */*
    if (elems[0] == "*")
      elems[0] = "*/*";

    // Split the media type into type and subtype
    auto mime_parts = split_trim(elems[0], '/');
    if (mime_parts.size() != 2 || mime_parts[0].empty() || mime_parts[1].empty()) {
      throw http::bad_request("Invalid accept header media type");
    }

    // */subtype is not allowed
    if (mime_parts[0] == "*" && mime_parts[1] != "*") {
      throw http::bad_request("Invalid wildcard in accept header media type");
    }

    // figure out the mime::type from the string
    mime::type mime_type = mime::parse_from(elems[0]);
    if (mime_type == mime::type::unspecified_type) {
      continue;
    }

    AcceptElement acceptElement;
    acceptElement.raw = elems[0];
    acceptElement.type = mime_parts[0];
    acceptElement.subtype = mime_parts[1];
    acceptElement.mimeType = mime_type;

    // Parse parameters
    for (size_t i = 1; i < elems.size(); ++i) {
      auto param_parts = split_trim(elems[i], '=');
      if (param_parts.size() != 2) {
        throw http::bad_request("Malformed parameter in accept header");
      }

      if (param_parts[0] == "q") {
        if (!parseQValue(param_parts[1], acceptElement.q)) {
          throw http::bad_request("Invalid q parameter value in accept header");
        }

        // Check if q is within the valid range [0, 1]
        if (!std::isfinite(acceptElement.q) || acceptElement.q < 0.0 || acceptElement.q > 1.0) {
            throw http::bad_request("Invalid q parameter value in accept header");
        }

      } else {
        acceptElement.params[std::string{param_parts[0]}] = std::string{param_parts[1]};
      }
    }

    acceptElements.push_back(acceptElement);
  }

  return acceptElements;
}

namespace {

/**
 * figures out the preferred mime type(s) from the Accept headers, mapped to
 * their relative acceptability.
 */
AcceptHeader header_mime_type(const request &req) {
  // need to look at HTTP_ACCEPT request environment
  std::string accept_header = fcgi_get_env(req, "HTTP_ACCEPT", "*/*");
  return AcceptHeader(accept_header);
}

std::string mime_types_to_string(const std::vector<mime::type> &mime_types) {
  if (mime_types.empty()) {
    return "";
  }

  std::string result = mime::to_string(mime_types[0]);
  for (size_t i = 1; i < mime_types.size(); ++i) {
    result += ", " + mime::to_string(mime_types[i]);
  }
  return result;
}

}  // anonymous namespace

mime::type choose_best_mime_type(const AcceptHeader& accept_header, const responder& hptr, const std::string& path) {
  const std::vector<mime::type> types_available = hptr.types_available();

  mime::type best_type = hptr.resource_type();
  // check if the handler is capable of supporting an acceptable set of mime types
  if (best_type != mime::type::unspecified_type) {
    // check that this doesn't conflict with anything in the Accept header
    if (!hptr.is_available(best_type)) {
      throw http::not_acceptable(fmt::format("Acceptable formats for {} are: {}",
                               path,
                               mime_types_to_string(types_available)));
    }
    if (!accept_header.is_acceptable(best_type)) {
      throw http::not_acceptable(fmt::format("Acceptable formats for {} are: {}",
                               path,
                               mime_types_to_string({best_type})));
    }
  } else {
    best_type = accept_header.most_acceptable_of(types_available);
    // if none were acceptable then...
    if (best_type == mime::type::unspecified_type) {
      throw http::not_acceptable(fmt::format("Acceptable formats for {} are: {}",
                               path,
                               mime_types_to_string(types_available)));
    }
    if (best_type == mime::type::any_type && !types_available.empty()) {
      // choose the first of the available types if nothing is preferred
      best_type = types_available.front();
    }
    // otherwise we've chosen the most acceptable and available type
  }

  return best_type;
}

mime::type choose_best_mime_type(const request &req, const responder& hptr) {
  // figure out what, if any, the Accept-able resource mime types are
  auto types = header_mime_type(req);
  auto path = get_request_path(req);
  return choose_best_mime_type(types, hptr, path);
}

std::unique_ptr<output_formatter> create_formatter(mime::type best_type, output_buffer& out) {

  switch (best_type) {
    case mime::type::application_xml:
      return std::make_unique<xml_formatter>(std::make_unique<xml_writer>(out, true));

    case mime::type::application_json:
      return std::make_unique<json_formatter>(std::make_unique<json_writer>(out, false));

    case mime::type::text_plain:
      return std::make_unique<text_formatter>(std::make_unique<text_writer>(out, true));

    default:
      throw std::runtime_error(fmt::format("Could not create formatter for MIME type `{}'.", mime::to_string(best_type)));
  }
}
