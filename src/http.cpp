/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/http.hpp"
#include "cgimap/options.hpp"
#include <vector>
#include <fmt/core.h>
#include <boost/algorithm/string.hpp>

#include <iterator> // for distance
#include <cctype>   // for toupper, isxdigit
#include <cstdlib>
#include <regex>
#include <sstream>

namespace al = boost::algorithm;
using std::string;
using std::vector;
using std::pair;

namespace {
/**
 * Functions hexToChar and form_urldecode were taken from GNU CGICC by
 * Stephen F. Booth and Sebastien Diaz, which is also released under the
 * GPL.
 */
char hexToChar(char first, char second) {
  int digit;

  digit = (first >= 'A' ? ((first & 0xDF) - 'A') + 10 : (first - '0'));
  digit *= 16;
  digit += (second >= 'A' ? ((second & 0xDF) - 'A') + 10 : (second - '0'));
  return static_cast<char>(digit);
}

std::string form_urldecode(const std::string &src) {
  std::string result;
  std::string::const_iterator iter;
  char c;

  for (iter = src.begin(); iter != src.end(); ++iter) {
    switch (*iter) {
    case '+':
      result.append(1, ' ');
      break;
    case '%':
      // Don't assume well-formed input
      if (std::distance(iter, src.end()) >= 2 && std::isxdigit(*(iter + 1)) &&
          std::isxdigit(*(iter + 2))) {
        c = *++iter;
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

exception::exception(int c, string m)
    : code_(c), message_(std::move(m)) {}

int exception::code() const { return code_; }

const char *exception::header() const { return status_message(code()); }

const char *exception::what() const noexcept { return message_.c_str(); }

server_error::server_error(const string &message)
    : exception(500, message) {}

bad_request::bad_request(const string &message)
    : exception(400, message) {}

forbidden::forbidden(const string &message)
    : exception(403, message) {}

not_found::not_found(const string &uri) 
    : exception(404, uri) {}

not_acceptable::not_acceptable(const string &message)
    : exception(406, message) {}

conflict::conflict(const string &message)
    : exception(409, message) {}

precondition_failed::precondition_failed(const string &message)
    : exception(412, message),
      fullstring("Precondition failed: " + message) {}

const char *precondition_failed::what() const noexcept { return fullstring.c_str(); }

payload_too_large::payload_too_large(const string &message)
    : exception(413, message) {}

too_many_requests::too_many_requests(const string &message)
    : exception(429, message) {}

bandwidth_limit_exceeded::bandwidth_limit_exceeded(int retry_seconds)
    : exception(509, fmt::format("You have downloaded too much data. Please try again in {} seconds.", retry_seconds)), retry_seconds(retry_seconds) {}

gone::gone(const string &message)
    : exception(410, message) {}

unsupported_media_type::unsupported_media_type(const string &message)
    : exception(415, message) {}

unauthorized::unauthorized(const std::string &message)
  : exception(401, message) {}

method_not_allowed::method_not_allowed(http::method method)
   :  exception(405, http::list_methods(method)),
      allowed_methods(method) {}


string urldecode(const string &s) { return form_urldecode(s); }

string urlencode(const string &s) {
  static const char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  std::ostringstream ostr;

  for (char c : s) {
    if (((c >= 'a') && (c <= 'z')) ||
        ((c >= 'A') && (c <= 'Z')) ||
        ((c >= '0') && (c <= '9')) ||
        (c == '-') ||
        (c == '.') ||
        (c == '_') ||
        (c == '~')) {
      ostr << c;

    } else {
      auto idx = (unsigned char)(c);
      ostr << "%" << hex[idx >> 4] << hex[idx & 0xf];
    }
  }

  std::string rv(ostr.str());
  return rv;
}

vector<pair<string, string> > parse_params(const string &p) {
  // Split the query string into components
  vector<pair<string, string> > queryKVPairs;
  if (!p.empty()) {
    vector<string> temp;
    al::split(temp, p, al::is_any_of("&"));

    for (const string &kvPair : temp) {
      vector<string> kvTemp;
      al::split(kvTemp, kvPair, al::is_any_of("="));

      if (kvTemp.size() == 2) {
        queryKVPairs.push_back(std::make_pair(kvTemp[0], kvTemp[1]));

      } else if (kvTemp.size() == 1) {
        queryKVPairs.push_back(std::make_pair(kvTemp[0], std::string()));
      }
    }
  }
  return queryKVPairs;
}

std::unique_ptr<encoding> choose_encoding(const string &accept_encoding) {
  vector<string> encodings;

  static const std::regex regex1("\\s*([^()<>@,;:\\\\\"/[\\]\\\\?={} "
      "\\t]+)\\s*;\\s*q\\s*=(\\d+(\\.\\d+)?)\\s*");

  static const std::regex regex2( R"(\s*([^()<>@,;:\\"/[\]\\?={} \t]+)\s*)");

  al::split(encodings, accept_encoding, al::is_any_of(","));

  float identity_quality = 0.001;
  float deflate_quality = 0.000;
  float gzip_quality = 0.000;
  float brotli_quality = 0.000;

  for (const string &encoding : encodings) {
    std::smatch what;
    string name;
    float quality;

    if (std::regex_match(
            encoding, what,
            regex1)) {
      name = what[1];
      quality = std::atof(string(what[2]).c_str());
    } else if (std::regex_match(
                   encoding, what,
                   regex2)) {
      name = what[1];
      quality = 1.0;
    } else {
      name = "";
      quality = 0.0;
    }

    if (al::iequals(name, "identity")) {
      identity_quality = quality;
    } else if (al::iequals(name, "deflate")) {
      deflate_quality = quality;
    } else if (al::iequals(name, "gzip")) {
      gzip_quality = quality;
    } else if (al::iequals(name, "br")) {
      brotli_quality = quality;
    } else if (al::iequals(name, "*")) {
      if (identity_quality == 0.000)
        identity_quality = quality;
      if (deflate_quality == 0.000)
        deflate_quality = quality;
      if (gzip_quality == 0.001)
        gzip_quality = quality;
      if (brotli_quality == 0.001)
        brotli_quality = quality;
    }
  }

#if HAVE_BROTLI
  if (brotli_quality > 0.0 && brotli_quality >= identity_quality &&
      brotli_quality >= deflate_quality &&
      brotli_quality >= gzip_quality) {
    return std::make_unique<brotli>();
  }
#endif
#ifdef HAVE_LIBZ
#ifdef ENABLE_DEFLATE
  if (deflate_quality > 0.0 && deflate_quality >= gzip_quality &&
      deflate_quality >= identity_quality) {
    return std:make_unique<deflate>();
  } else
#endif /* ENABLE_DEFLATE */
      if (gzip_quality > 0.0 && gzip_quality >= identity_quality) {
    return std::make_unique<gzip>();
  }
#endif /* HAVE_LIBZ */
  else if (identity_quality > 0.0) {
    return std::make_unique<identity>();
  } else {
    throw http::not_acceptable("No acceptable content encoding found. Only "
                               "identity and gzip are supported.");
  }
}

std::unique_ptr<ZLibBaseDecompressor> get_content_encoding_handler(const std::string &content_encoding) {

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
  std::ostringstream result;

  bool first = true;
  for (auto const &pair : METHODS) {
    if ((m & pair.first) == pair.first) {
      if (first) { first = false; } else { result << ", "; }
      result << pair.second;
    }
  }
  return result.str();
}

std::optional<method> parse_method(const std::string &s) {
  std::optional<method> result;

  for (auto const &pair : METHODS) {
    if (pair.second == s) {
      result = pair.first;
      break;
    }
  }
  return result;
}

std::ostream &operator<<(std::ostream &out, method m) {
  std::string s = list_methods(m);
  out << "methods{" << s << "}";
  return out;
}

unsigned long parse_content_length(const std::string &content_length_str) {

  char *end;

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

} // namespace http
