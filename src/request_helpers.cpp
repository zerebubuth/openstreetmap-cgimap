#include "cgimap/request_helpers.hpp"
#include <sstream>
#include <cstring>

using std::string;
using std::ostringstream;

string fcgi_get_env(request &req, const char *name, const char *default_value) {
  assert(name);
  const char *v = req.get_param(name);

  // since the map script is so simple i'm just going to assume that
  // any time we fail to get an environment variable is a fatal error.
  if (v == NULL) {
    if (default_value) {
      v = default_value;
    } else {
      ostringstream ostr;
      ostr << "request didn't set the $" << name << " environment variable.";
      throw http::server_error(ostr.str());
    }
  }

  return string(v);
}

string get_query_string(request &req) {
  // try the query string that's supposed to be present first
  const char *query_string = req.get_param("QUERY_STRING");

  // if that isn't present, then this may be being invoked as part of a
  // 404 handler, so look at the request uri instead.
  if (query_string == NULL || strlen(query_string) == 0) {
    const char *request_uri = req.get_param("REQUEST_URI");

    if ((request_uri == NULL) || (strlen(request_uri) == 0)) {
      // fail. something has obviously gone massively wrong.
      ostringstream ostr;
      ostr << "request didn't set the $QUERY_STRING or $REQUEST_URI "
           << "environment variables.";
      throw http::server_error(ostr.str());
    }

    const char *request_uri_end = request_uri + strlen(request_uri);
    // i think the only valid position for the '?' char is at the beginning
    // of the query string.
    const char *question_mark = std::find(request_uri, request_uri_end, '?');
    if (question_mark == request_uri_end) {
      return string();
    } else {
      return string(question_mark + 1);
    }

  } else {
    return string(query_string);
  }
}

std::string get_request_path(request &req) {
  const char *request_uri = req.get_param("REQUEST_URI");

  if ((request_uri == NULL) || (strlen(request_uri) == 0)) {
    // fall back to PATH_INFO if REQUEST_URI isn't available.
    // the former is set by fcgi, the latter by Rack.
    request_uri = req.get_param("PATH_INFO");
  }

  if ((request_uri == NULL) || (strlen(request_uri) == 0)) {
    ostringstream ostr;
    ostr << "request didn't set either the $REQUEST_URI or $PATH_INFO "
            "environment variables.";
    throw http::server_error(ostr.str());
  }

  const char *request_uri_end = request_uri + strlen(request_uri);
  // i think the only valid position for the '?' char is at the beginning
  // of the query string.
  const char *question_mark = std::find(request_uri, request_uri_end, '?');
  if (question_mark == request_uri_end) {
    return string(request_uri);
  } else {
    return string(request_uri, question_mark);
  }
}

/**
 * get encoding to use for response.
 */
boost::shared_ptr<http::encoding> get_encoding(request &req) {
  const char *accept_encoding = req.get_param("HTTP_ACCEPT_ENCODING");

  if (accept_encoding) {
    return http::choose_encoding(string(accept_encoding));
  } else {
    return boost::shared_ptr<http::identity>(new http::identity());
  }
}

namespace {
const char *http_message_status_200 = "OK";
const char *http_message_status_400 = "Bad Request";
const char *http_message_status_401 = "Unauthorized";
const char *http_message_status_404 = "Not Found";
const char *http_message_status_405 = "Method Not Allowed";
const char *http_message_status_406 = "Not Acceptable";
const char *http_message_status_410 = "Gone";
const char *http_message_status_509 = "Bandwidth Limit Exceeded";
const char *http_message_status_500 = "Internal Server Error";
} // anonymous namespace

const char *status_message(int code) {
  const char *msg = http_message_status_500;

  switch (code) {
  case 200:
    msg = http_message_status_200;
    break;
  case 400:
    msg = http_message_status_400;
    break;
  case 401:
    msg = http_message_status_401;
    break;
  case 404:
    msg = http_message_status_404;
    break;
  case 405:
    msg = http_message_status_405;
    break;
  case 406:
    msg = http_message_status_406;
    break;
  case 410:
    msg = http_message_status_410;
    break;
  case 509:
    msg = http_message_status_509;
    break;
  }

  return msg;
}

namespace {
/**
 * Bindings to allow libxml to write directly to the request
 * library.
 */
class fcgi_output_buffer : public output_buffer {
public:
  virtual int write(const char *buffer, int len) {
    w += len;
    return r.put(buffer, len);
  }

  virtual int close() {
    // we don't actually close the request output, as that happens
    // automatically on the next call to accept.
    return 0;
  }

  virtual int written() { return w; }

  virtual void flush() {
    // there's a note that says this causes too many writes and decreases
    // efficiency, but we're only calling it once...
    r.flush();
  }

  virtual ~fcgi_output_buffer() {}

  fcgi_output_buffer(request &req) : r(req), w(0) {}

private:
  request &r;
  int w;
};

} // anonymous namespace

boost::shared_ptr<output_buffer> make_output_buffer(request &req) {
  return boost::shared_ptr<output_buffer>(new fcgi_output_buffer(req));
}
