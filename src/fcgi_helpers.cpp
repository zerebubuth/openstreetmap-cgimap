#include "fcgi_helpers.hpp"
#include "http.hpp"
#include <sstream>
#include <cstring>

using std::string;
using std::ostringstream;

string
fcgi_get_env(FCGX_Request &req, const char* name, const char* default_value) {
  assert(name);
  const char* v = FCGX_GetParam(name, req.envp);

  // since the map script is so simple i'm just going to assume that
  // any time we fail to get an environment variable is a fatal error.
  if (v == NULL) {
    if (default_value) {
      v = default_value;
    } else {
      ostringstream ostr;
      ostr << "FCGI didn't set the $" << name << " environment variable.";
      throw http::server_error(ostr.str());
    }
  }

  return string(v);
}

string
get_query_string(FCGX_Request &req) {
  // try the query string that's supposed to be present first
  const char *query_string = FCGX_GetParam("QUERY_STRING", req.envp);
  
  // if that isn't present, then this may be being invoked as part of a
  // 404 handler, so look at the request uri instead.
  if ((query_string == NULL) || (strlen(query_string) == 0)) {
    const char *request_uri = FCGX_GetParam("REQUEST_URI", req.envp);

    if ((request_uri == NULL) || (strlen(request_uri) == 0)) {
      // fail. something has obviously gone massively wrong.
      ostringstream ostr;
      ostr << "FCGI didn't set the $QUERY_STRING or $REQUEST_URI "
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

std::string
get_request_path(FCGX_Request &req) {
  const char *request_uri = FCGX_GetParam("REQUEST_URI", req.envp);
  
  if ((request_uri == NULL) || (strlen(request_uri) == 0)) {
    ostringstream ostr;
    ostr << "FCGI didn't set the $REQUEST_URI environment variable.";
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
