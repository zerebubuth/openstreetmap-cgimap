/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/request_helpers.hpp"
#include "cgimap/zlib.hpp"

#include <cassert>
#include <cstring>

#include <fmt/core.h>


std::string fcgi_get_env(const request &req, const char *name, const char *default_value) {
  assert(name);
  const char *v = req.get_param(name);

  // since the map script is so simple i'm just going to assume that
  // any time we fail to get an environment variable is a fatal error.
  if (v == nullptr) {
    if (default_value) {
      v = default_value;
    } else {
      throw http::server_error(fmt::format("request didn't set the ${} environment variable.", name));
    }
  }

  return std::string(v);
}

std::string get_query_string(const request &req) {
  // try the query string that's supposed to be present first
  auto *query_string = req.get_param("QUERY_STRING");

  // if that isn't present, then this may be being invoked as part of a
  // 404 handler, so look at the request uri instead.
  if (query_string == nullptr || strlen(query_string) == 0) {
    const char *request_uri = req.get_param("REQUEST_URI");

    if ((request_uri == nullptr) || (strlen(request_uri) == 0)) {
      // fail. something has obviously gone massively wrong.
      throw http::server_error("request didn't set the $QUERY_STRING or $REQUEST_URI environment variables.");
    }

    const char *request_uri_end = request_uri + strlen(request_uri);
    // i think the only valid position for the '?' char is at the beginning
    // of the query string.
    auto *question_mark = std::find(request_uri, request_uri_end, '?');
    if (question_mark == request_uri_end) {
      return {};
    } else {
      return {question_mark + 1};
    }

  } else {
    return {query_string};
  }
}

std::string get_request_path(const request &req) {
  const char *request_uri = req.get_param("REQUEST_URI");

  if ((request_uri == nullptr) || (strlen(request_uri) == 0)) {
    // fall back to PATH_INFO if REQUEST_URI isn't available.
    // the former is set by fcgi, the latter by Rack.
    request_uri = req.get_param("PATH_INFO");
  }

  if ((request_uri == nullptr) || (strlen(request_uri) == 0)) {
    throw http::server_error("request didn't set the $QUERY_STRING or $REQUEST_URI environment variables.");
  }

  const char *request_uri_end = request_uri + strlen(request_uri);
  // i think the only valid position for the '?' char is at the beginning
  // of the query string.
  auto *question_mark = std::find(request_uri, request_uri_end, '?');
  if (question_mark == request_uri_end) {
    return {request_uri};
  } else {
    return {request_uri, question_mark};
  }
}

/**
 * get encoding to use for response.
 */
std::unique_ptr<http::encoding> get_encoding(const request &req) {
  const char *accept_encoding = req.get_param("HTTP_ACCEPT_ENCODING");

  if (accept_encoding) {
    return http::choose_encoding(std::string(accept_encoding));
  } else {
    return std::make_unique<http::identity>();
  }
}


namespace {
/**
 * Bindings to allow libxml to write directly to the request
 * library.
 */
class fcgi_output_buffer : public output_buffer {
public:
  int write(const char *buffer, int len) override {
    w += len;
    return r.put(buffer, len);
  }

  int close() override {
    // we don't actually close the request output, as that happens
    // automatically on the next call to accept.
    return 0;
  }

  [[nodiscard]] int written() const override { return w; }

  void flush() override {
    // there's a note that says this causes too many writes and decreases
    // efficiency, but we're only calling it once...
    r.flush();
  }

  ~fcgi_output_buffer() override = default;

  explicit fcgi_output_buffer(request &req) : r(req) {}

  fcgi_output_buffer(const fcgi_output_buffer&) = delete;
  fcgi_output_buffer& operator=(const fcgi_output_buffer&) = delete;
  fcgi_output_buffer(fcgi_output_buffer&&) = delete;
  fcgi_output_buffer& operator=(fcgi_output_buffer&&) = delete;

private:
  request &r;
  int w{0};
};

} // anonymous namespace

std::unique_ptr<output_buffer> make_output_buffer(request &req) {
  return std::make_unique<fcgi_output_buffer>(req);
}
