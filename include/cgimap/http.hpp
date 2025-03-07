/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef HTTP_HPP
#define HTTP_HPP

#include <memory>
#include <string>
#include <vector>
#include <exception>
#include <optional>
#include <ostream>

#ifdef HAVE_LIBZ
#include "cgimap/zlib.hpp"
#endif
#if HAVE_BROTLI
#include "cgimap/brotli.hpp"
#endif

#include "cgimap/output_buffer.hpp"

/**
 * Contains the generic HTTP methods and classes involved in the
 * application. CGI-specific stuff is elsewhere, but this stuff
 * should be theoretically re-usable in a non CGI context.
 */
namespace http {

  enum class method : uint8_t {
    GET     = 0b00001,
    POST    = 0b00010,
    PUT     = 0b00100,
    HEAD    = 0b01000,
    OPTIONS = 0b10000
  };

  using headers_t = std::vector<std::pair<std::string, std::string> >;

  std::string format_header(int status, const headers_t &headers);

  /**
   * return a static string description for an HTTP status code.
   */
  const char *status_message(int code);


/**
 * Base class for HTTP protocol related exceptions.
 *
 * Not directly constructable - use the derived classes instead.
 */
class exception : public std::exception {
private:
  /// numerical status code, for more information see
  /// http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
  const int code_;

  /// specific error message, meant entirely for humans to read.
  const std::string message_;

protected:
  exception(int c, std::string m);

public:
  ~exception() noexcept override = default;

  int code() const;
  const char* header() const;
  const char* what() const noexcept override;
};

/**
 * An error which has caused the current request to fail which is
 * due to an internal error or code bug. The client might try again,
 * but there is no guarantee it will work. Use only for conditions
 * which are unrecoverable.
 */
class server_error : public exception {
public:
  explicit server_error(const std::string &message);
};

/**
 * The client's request is badly-formed and cannot be serviced. Used
 * mainly for parse errors, or invalid data.
 */
class bad_request : public exception {
public:
  explicit bad_request(const std::string &message);
};

/**
 * The server understood the request, but is refusing to fulfill it.
 * Authorization will not help and the request SHOULD NOT be repeated.
 */

class forbidden : public exception {
public:
   explicit forbidden(const std::string &message);
};

/**
 * The client has attempted to use an HTTP method which is not
 * supported on the receiving URI. This is a very specific error
 * and shouldn't be used except in this context.
 */
class method_not_allowed : public exception {
public:
  explicit method_not_allowed(http::method method);
  http::method allowed_methods;
};

/**
 * Content negotiation failed to find a matching format which
 * the server is able to return and which the client is prepared
 * to accept.
 */
class not_acceptable : public exception {
public:
  explicit not_acceptable(const std::string &message);
};

/**
 * Indicates that the request could not be processed because of conflict
 * in the request, such as an edit conflict between
 * multiple simultaneous updates.
 */
class conflict : public exception {
public:
  explicit conflict(const std::string &message);
};

/**
 * The server does not meet one of the preconditions that
 * the requester put on the request.
 */
class precondition_failed : public exception {
public:
  explicit precondition_failed(const std::string &message);
  const char *what() const noexcept override;
private:
  std::string fullstring;
};

/**
 * The request is larger than the server is willing or able to process.
 * Previously called "Request Entity Too Large"
 */

class payload_too_large : public exception {
public:
  explicit payload_too_large(const std::string &message);
};


/**
 * The HTTP 429 Too Many Requests response status code indicates
 * the user has sent too many requests in a given amount of time ("rate limiting").
 */

class too_many_requests : public exception {
public:
  explicit too_many_requests(const std::string &message);
};


/**
 * The request resource could not be found, or is not handled
 * by the server.
 */
class not_found : public exception {
public:
  explicit not_found(const std::string &uri);
};

/**
 * The client has exceeded the bandwidth limit.
 */
class bandwidth_limit_exceeded : public exception {
public:
  explicit bandwidth_limit_exceeded(int retry_seconds);
  int retry_seconds;
};

/**
 * Indicates that a resource which was previously present has been deleted.
 */
class gone : public exception {
public:
  // TODO: fix up so that error message is meaningful
  explicit gone(const std::string &message = "");
};

/**
 * Indicates that the client is not authorized to perform the request.
 */
class unauthorized : public exception {
public:
  explicit unauthorized(const std::string &message);
};

/**
 * The origin server is refusing to service the request because the payload
 * is in a format not supported by this method on the target resource.
 */
class unsupported_media_type : public exception {
public:
  explicit unsupported_media_type(const std::string &message);
};

/**
 * Decodes a url-encoded string.
 */
std::string urldecode(const std::string &s);

/**
 * Encodes a string to canonical url-encoding.
 *
 * This is compatible with the OAuth 1.0a definition of url-encoding.
 */
std::string urlencode(const std::string &s);

/**
 * Parses a query string into an array of key-value pairs.
 *
 * Note that it can be important to keep the parameters as an array of key-value
 * pairs, since it is possible to have duplicate parameters - for example, in
 * https://tools.ietf.org/html/rfc5849#section-3.4.1.3.1
 *
 * Behaviour of duplicate items in the query string seems undefined in
 * https://tools.ietf.org/html/rfc3986#section-3.4, and the above example has
 * the duplicate in the form-encoded POST body. It seems best to never use
 * duplicates in request parameters, but hopefully this code is now robust to
 * their existence.
 *
 * The string should already have been url-decoded (i.e: no %-encoded
 * chars remain).
 */
std::vector<std::pair<std::string, std::string> > parse_params(const std::string &p);

/*
 * HTTP Content Encodings.
 */
class encoding {
private:
  const std::string name_;

public:
  explicit encoding(std::string name) : name_(std::move(name)){}
  virtual ~encoding() = default;

  const std::string &name() const { return name_; };

  virtual std::unique_ptr<output_buffer>  buffer(output_buffer& out) {
    return std::make_unique<identity_output_buffer>(out);
  }
};

class identity : public encoding {
public:
  identity() : encoding("identity"){};
};

#ifdef HAVE_LIBZ
class deflate : public encoding {
public:
  deflate() : encoding("deflate"){}

  std::unique_ptr<output_buffer> buffer(output_buffer& out) override {
    return std::make_unique<zlib_output_buffer>(out, zlib_output_buffer::zlib);
  }
};

class gzip : public encoding {
public:
  gzip() : encoding("gzip"){}
  std::unique_ptr<output_buffer> buffer(output_buffer& out) override {
    return std::make_unique<zlib_output_buffer>(out, zlib_output_buffer::gzip);
  }
};
#endif /* HAVE_LIBZ */

#if HAVE_BROTLI

class brotli : public encoding {
public:
  brotli() : encoding("br"){}
  std::unique_ptr<output_buffer> buffer(output_buffer& out) override {
    return std::make_unique<brotli_output_buffer>(out);
  }
};
#endif

/*
 * Parses an Accept-Encoding header and returns the chosen
 * encoding.
 */
std::unique_ptr<http::encoding> choose_encoding(const std::string &accept_encoding);

std::unique_ptr<ZLibBaseDecompressor> get_content_encoding_handler(const std::string &content_encoding);



// allow bitset-like operators on methods
constexpr method operator|(method a, method b) {
  return static_cast<method>(static_cast<std::underlying_type_t<method>>(a) |
                             static_cast<std::underlying_type_t<method>>(b));
}
constexpr method operator&(method a, method b) {
  return static_cast<method>(static_cast<std::underlying_type_t<method>>(a) &
                             static_cast<std::underlying_type_t<method>>(b));
}
constexpr method& operator|=(method& a, method b)
{
  return a= a | b;
}

// return a comma-delimited string describing the methods.
std::string list_methods(method m);

// parse a single method string into a http::method enum, or return none
// if it's not a known value.
std::optional<method> parse_method(const std::string &);

// parse CONTENT_LENGTH HTTP header
unsigned long parse_content_length(const std::string &);

std::ostream &operator<<(std::ostream &, method);

} // namespace http

#endif /* HTTP_HPP */
