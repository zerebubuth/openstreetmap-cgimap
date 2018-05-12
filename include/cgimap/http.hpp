#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>
#include <vector>
#include <bitset>
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <ostream>
#include "cgimap/config.hpp"

#ifdef HAVE_LIBZ
#include "cgimap/zlib.hpp"
#endif
#include "cgimap/output_buffer.hpp"

/**
 * Contains the generic HTTP methods and classes involved in the
 * application. CGI-specific stuff is elsewhere, but this stuff
 * should be theoretically re-usable in a non CGI context.
 */
namespace http {

/**
 * Base class for HTTP protocol related exceptions.
 *
 * Not directly constructable - use the derived classes instead.
 */
class exception : public std::exception {
private:
  /// numerical status code, for more information see
  /// http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
  const unsigned int code_;

  /// the header is a short description of the code, mainly for
  /// human consumption.
  const std::string header_;

  /// specific error message, meant entirely for humans to read.
  const std::string message_;

protected:
  exception(unsigned int c, const std::string &h, const std::string &m);

public:
  virtual ~exception() throw();

  unsigned int code() const;
  const std::string &header() const;
  const char *what() const throw();
};

/**
 * An error which has caused the current request to fail which is
 * due to an internal error or code bug. The client might try again,
 * but there is no guarantee it will work. Use only for conditions
 * which are unrecoverable.
 */
class server_error : public exception {
public:
  server_error(const std::string &message);
};

/**
 * The client's request is badly-formed and cannot be serviced. Used
 * mainly for parse errors, or invalid data.
 */
class bad_request : public exception {
public:
  bad_request(const std::string &message);
};

/**
 * The client has attempted to use an HTTP method which is not
 * supported on the receiving URI. This is a very specific error
 * and shouldn't be used except in this context.
 */
class method_not_allowed : public exception {
public:
  method_not_allowed(const std::string &message);
};

/**
 * Content negotiation failed to find a matching format which
 * the server is able to return and which the client is prepared
 * to accept.
 */
class not_acceptable : public exception {
public:
  not_acceptable(const std::string &message);
};

/**
 * Indicates that the request could not be processed because of conflict
 * in the request, such as an edit conflict between
 * multiple simultaneous updates.
 */
class conflict : public exception {
public:
  conflict(const std::string &message);
};

/**
 * The server does not meet one of the preconditions that
 * the requester put on the request.
 */
class precondition_failed : public exception {
public:
  precondition_failed(const std::string &message);
};

/**
 * The request is larger than the server is willing or able to process.
 * Previously called "Request Entity Too Large"
 */

class payload_too_large : public exception {
public:
  payload_too_large(const std::string &message);
};


/**
 * The request resource could not be found, or is not handled
 * by the server.
 */
class not_found : public exception {
public:
  not_found(const std::string &uri);
};

/**
 * The client has exceeded the bandwidth limit.
 */
class bandwidth_limit_exceeded : public exception {
public:
  bandwidth_limit_exceeded(const std::string &message);
};

/**
 * Indicates that a resource which was previously present has been deleted.
 */
class gone : public exception {
public:
  // TODO: fix up so that error message is meaningful
  gone(const std::string &message = "");
};

/**
 * Indicates that the client is not authorized to perform the request.
 */
class unauthorized : public exception {
public:
  unauthorized(const std::string &message);
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
  encoding(const std::string &name) : name_(name){};
  virtual ~encoding(void){};
  const std::string &name(void) const { return name_; };
  virtual boost::shared_ptr<output_buffer>
  buffer(boost::shared_ptr<output_buffer> out) {
    return out;
  }
};

class identity : public encoding {
public:
  identity(void) : encoding("identity"){};
};

#ifdef HAVE_LIBZ
class deflate : public encoding {
public:
  deflate(void) : encoding("deflate"){};
  virtual boost::shared_ptr<output_buffer>
  buffer(boost::shared_ptr<output_buffer> out) {
    return boost::shared_ptr<zlib_output_buffer>(
        new zlib_output_buffer(out, zlib_output_buffer::zlib));
  }
};

class gzip : public encoding {
public:
  gzip(void) : encoding("gzip"){};
  virtual boost::shared_ptr<output_buffer>
  buffer(boost::shared_ptr<output_buffer> out) {
    return boost::shared_ptr<zlib_output_buffer>(
        new zlib_output_buffer(out, zlib_output_buffer::gzip));
  }
};
#endif /* HAVE_LIBZ */

/*
 * Parses an Accept-Encoding header and returns the chosen
 * encoding.
 */
boost::shared_ptr<http::encoding>
choose_encoding(const std::string &accept_encoding);

enum class method : uint8_t {
  GET     = 0b0001,
  POST    = 0b0010,
  HEAD    = 0b0100,
  OPTIONS = 0b1000
};

// allow bitset-like operators on methods
inline method operator|(method a, method b) {
  return static_cast<method>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline method operator&(method a, method b) {
  return static_cast<method>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

// return a comma-delimited string describing the methods.
std::string list_methods(method m);

// parse a single method string into a http::method enum, or return boost::none
// if it's not a known value.
boost::optional<method> parse_method(const std::string &);

// parse CONTENT_LENGTH HTTP header
unsigned long parse_content_length(const std::string &);

std::ostream &operator<<(std::ostream &, method);

} // namespace http

#endif /* HTTP_HPP */
