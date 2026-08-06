/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/http.hpp"
#include "cgimap/options.hpp"
#include "cgimap/util.hpp"
#include <vector>
#include <fmt/core.h>

#include <iterator> // for distance
#include <cctype>   // for toupper, isxdigit
#include <cstdlib>
#include <ranges>
#include <string_view>
#include <algorithm>


namespace {
/**
 * Functions hexToChar and form_urldecode were taken from GNU CGICC by
 * Stephen F. Booth and Sebastien Diaz, which is also released under the
 * GPL.
 */
char hexToChar(char first, char second) {

  int digit = (first >= 'A' ? ((first & 0xDF) - 'A') + 10 : (first - '0'));
  digit *= 16;
  digit += (second >= 'A' ? ((second & 0xDF) - 'A') + 10 : (second - '0'));
  return static_cast<char>(digit);
}

std::string form_urldecode(const std::string &src) {
  std::string result;
  std::string::const_iterator iter;

  for (iter = src.begin(); iter != src.end(); ++iter) {
    switch (*iter) {
    case '+':
      result.append(1, ' ');
      break;
    case '%':
      // Don't assume well-formed input
      if (std::distance(iter, src.end()) >= 2 && std::isxdigit(*(iter + 1)) &&
          std::isxdigit(*(iter + 2))) {
        char c = *++iter;
        result.append(1, hexToChar(c, *++iter));
      }
      // Just pass the % through untouched
      else {
        result.append(1, '%');
      }
      break;

    default:
      result.append(1, *iter);
      break;
    }
  }

  return result;
}
}

namespace http {

const char *status_message(int code) {

  switch (code) {
  case 200:
    return "OK";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 406:
    return "Not Acceptable";
  case 409:
    return "Conflict";
  case 410:
    return "Gone";
  case 412:
    return "Precondition Failed";
  case 413:
    return "Payload Too Large";
  case 415:
    return "Unsupported Media Type";
  case 429:
    return "Too Many Requests";
  case 509:
    return "Bandwidth Limit Exceeded";
  default:
    return "Internal Server Error";
  }
}

std::string format_header(int status, const headers_t &headers) {
  std::string hdr{};
  hdr += fmt::format("Status: {} {}\r\n", status, status_message(status));
  for (const auto& [name, value] : headers) {
    hdr += fmt::format("{}: {}\r\n", name, value);
  }
  hdr += "\r\n";
  return hdr;
}

int exception::code() const { return code_; }
const char *exception::header() const { return status_message(code()); }

bandwidth_limit_exceeded::bandwidth_limit_exceeded(int retry_seconds)
    : exception(509, fmt::format("You have downloaded too much data. Please try again in {} seconds.", retry_seconds)), retry_seconds(retry_seconds) {}

gone::gone(const std::string &message)
    : exception(410, message) {}

gone::gone(const char *message)
    : exception(410, message) {}

method_not_allowed::method_not_allowed(http::method method)
   :  exception(405, http::list_methods(method)),
      allowed_methods(method) {}

std::string urldecode(const std::string &s) { return form_urldecode(s); }

std::vector<std::pair<std::string, std::string>> parse_params(const std::string &p) {
  // Split the query string into components
  std::vector<std::pair<std::string, std::string>> queryKVPairs;
  if (!p.empty()) {
    auto temp = split(p, '&');

    for (const auto &kvPair : temp) {
      auto kvTemp = split(kvPair, '=');

      if (kvTemp.size() == 2) {
        queryKVPairs.emplace_back(std::string{kvTemp[0]}, std::string{kvTemp[1]});

      } else if (kvTemp.size() == 1) {
        queryKVPairs.emplace_back(std::string{kvTemp[0]}, std::string());
      }
    }
  }
  return queryKVPairs;
}

static const std::vector<const encoding*> supported_encodings = {{
  // Order of elements determines chosen encoding for * wildcard value
  #if HAVE_BROTLI
  &brotli::instance(),
  #endif
  #ifdef HAVE_LIBZ
  &gzip::instance(),
  &deflate::instance(),
  #endif
  &identity::instance()
}};

const std::vector<const encoding*>& get_supported_encodings() {
  return supported_encodings;
}

const encoding* choose_encoding(std::string_view accept_encoding) {
  std::vector<std::pair<const encoding*, float>> potential_encodings{};

  HttpListView accept_encodings{accept_encoding};
  
  for (const auto& encoding_id : accept_encodings.values) {
    for (const auto& enc : get_supported_encodings()) {
      if (enc->matches(encoding_id.item)) {
        float q = 1.0;

        for (const auto& param : encoding_id.parameters) {
          if (param.key == "q") {
            auto [last_parsed, ec] = std::from_chars(param.value.begin(), param.value.end(), q);

            if ((ec != std::errc()) || 
                (last_parsed != param.value.end()) || 
                !(std::isfinite(q) && 0.0 <= q && q <= 1.0)) {
              throw http::bad_request("Malformed encoding quality value parameter");
            }
            break;
          }
        }

        if (q == 1.0) {
          // Short circuit using first accepted encoding with highest quality 1.0
          return enc;
        }
        potential_encodings.emplace_back(enc, q);
      }
    }
  }

  std::ranges::stable_sort(potential_encodings, 
                           std::ranges::greater(), 
                           &std::pair<const encoding*, float>::second);

  // Find first non 0 quality encoding
  auto identity_encoding = &http::identity::instance();
  bool identity_is_acceptable = true;
  for (const auto& enc : potential_encodings) {
    if (enc.second != 0.0) {
      return enc.first;
    } else if(enc.first->matches(identity_encoding->name())) {
      identity_is_acceptable = false;
    }
  }

  if (!identity_is_acceptable) {
    throw http::not_acceptable("No acceptable content encoding found.");
  }

  return identity_encoding;
}

std::unique_ptr<ZLibBaseDecompressor> get_content_encoding_handler(std::string_view content_encoding) {

  if (content_encoding.empty())
    return std::make_unique<IdentityDecompressor>();

  if (content_encoding == "identity")
      return std::make_unique<IdentityDecompressor>();
#ifdef HAVE_LIBZ
  else if (content_encoding == "gzip")
    return std::make_unique<GZipDecompressor>();
  else if (content_encoding == "deflate")
    return std::make_unique<ZLibDecompressor>();
  throw http::unsupported_media_type("Supported Content-Encodings include 'gzip' and 'deflate'");

#else
  throw http::unsupported_media_type("Supported Content-Encodings are 'identity'");
#endif
}

namespace {

const std::map<method, std::string> METHODS = {
  {method::GET,     "GET"},
  {method::POST,    "POST"},
  {method::PUT,     "PUT"},
  {method::HEAD,    "HEAD"},
  {method::OPTIONS, "OPTIONS"}
};

} // anonymous namespace

std::string list_methods(method m) {
  std::string result;

  for (auto const &pair : METHODS) {
    if ((m & pair.first) == pair.first) {
      if (!result.empty()) {
        result += ", ";
      }
      result += pair.second;
    }
  }
  return result;
}

std::optional<method> parse_method(std::string_view s) {

  for (auto const &pair : METHODS) {
    if (pair.second == s) {
      return pair.first;
    }
  }
  return {};
}

unsigned long parse_content_length(const std::string &content_length_str) {

  char *end = nullptr;

  const long length = strtol(content_length_str.c_str(), &end, 10);

  if (end == content_length_str) {
    throw http::bad_request("CONTENT_LENGTH not a decimal number");
  } else if ('\0' != *end) {
    throw http::bad_request("CONTENT_LENGTH: extra characters at end of input");
  } else if (length < 0) {
    throw http::bad_request("CONTENT_LENGTH: invalid value");
  } else if (length > global_settings::get_payload_max_size())
    throw http::payload_too_large(fmt::format("CONTENT_LENGTH exceeds limit of {:d} bytes", global_settings::get_payload_max_size()));

  return length;
}


http::HttpParameterView http::HttpParameterView::parse(std::string_view param) {
  auto param_parts = param 
    | std::ranges::views::split('=') 
    | std::ranges::views::transform([](const auto& x){ return trim(std::string_view{x.begin(), x.end()}); });

  auto param_begin = param_parts.begin();
  auto param_end = param_parts.end();

  std::string_view param_key = pop_or_throw(param_begin, param_end, http::bad_request("Missing parameter key in http header list"));
  if (param_key.empty()) {
    throw http::bad_request("Empty parameter key in http header list");
  }
  std::string_view param_value = pop_or_throw(param_begin, param_end, http::bad_request("Missing parameter value in http header list"));
  if (param_value.empty()) {
    throw http::bad_request("Empty parameter value in http header list");
  }

  if (param_begin != param_end) {
    throw http::bad_request("Malformed parameter in http header list");
  }

  return {param_key, param_value};
}

http::HttpItemView http::HttpItemView::parse(std::string_view list_value) {
  auto params = list_value 
    | std::ranges::views::split(';') 
    | std::ranges::views::transform([](const auto& x){ return trim(std::string_view{x.begin(), x.end()}); });

  if (std::ranges::empty(params)) {
    throw http::bad_request("Malformed http header list value");
  }

  auto item = params.front();

  if (item.empty()) {
    throw http::bad_request("Malformed http header list value");
  }

  auto xs = params
    | std::views::drop(1)
    | std::views::transform(http::HttpParameterView::parse);

  return http::HttpItemView{item, {xs.begin(), xs.end()}};
}

} // namespace http
