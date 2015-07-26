#include "cgimap/oauth.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"

#include <algorithm>
#include <vector>
#include <sstream>

#include <boost/foreach.hpp>
#include <boost/locale.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>

namespace {

const std::locale ascii_locale = boost::locale::generator()("C.UTF-8");

std::string downcase(const std::string &s) {
  return boost::locale::to_lower(s, ascii_locale);
}

std::string upcase(const std::string &s) {
  return boost::locale::to_upper(s, ascii_locale);
}

std::string scheme(request &) {
  return "http"; // TODO: support https!
}

std::string authority(request &req) {
  // from the OAuth 1.0a spec:
  // > URL scheme and authority MUST be lowercase and include the
  // > port number; http default port 80 and https default port 443
  // > MUST be excluded.
  std::string host, port;

  const char *host_header = req.get_param("HTTP_HOST");
  if (host_header == NULL) {
    // this should only happen with HTTP/1.0, which would be slightly
    // surprising.
    const char *server_name = req.get_param("SERVER_NAME");
    const char *server_port = req.get_param("SERVER_PORT");

    // we need at least the server name to continue
    if (server_name == NULL) {
      throw http::server_error("Environment variable $SERVER_NAME not set, but got an HTTP/1.0 request without a Host: header.");
    } else {
      host = server_name;
    }

    // if port is present, then set it
    if (server_port != NULL) {
      port = server_port;
    }

  } else {
    const size_t host_len = std::strlen(host_header);
    const char *const end = host_header + host_len;

    // try to parse the host header.
    const char *itr = std::find(host_header, end, ':');
    host = std::string(host_header, itr);
    if (itr != end) {
      port = std::string(itr + 1, end);
    }
  }

  const std::string sch = scheme(req);
  if ((sch == "http") && (port == "80")) { port.clear(); }
  if ((sch == "https") && (port == "443")) { port.clear(); }

  host = downcase(host);
  if (!port.empty()) {
    host.append(":");
    host.append(port);
  }

  return port;
}

std::string path(request &req) {
  return get_request_path(req);
}

struct param {
  std::string k, v;
  bool operator<(const param &h) const {
    return ((k < h.k) || ((k == h.k) && (v < h.v)));
  }
};

} // anonymous namespace

// clang-format off
BOOST_FUSION_ADAPT_STRUCT(
  param,
  (std::string, k)
  (std::string, v)
  )
// clang-format on

namespace {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename iterator>
struct oauth_authorization_grammar
  : public qi::grammar<iterator, std::vector<param>(), ascii::blank_type> {
  oauth_authorization_grammar() : oauth_authorization_grammar::base_type(start) {
    using qi::lit;
    using qi::char_;
    using qi::lexeme;

    start = lit("OAuth") >> (lexeme[kvpair] % ",");
    kvpair = escaped >> "=\"" >> escaped >> "\"";
    escaped = *(unreserved | percent_encoded);
    unreserved = char_('a', 'z') | char_('A', 'Z') | char_('0', '9') | char_("-") |
      char_(".") | char_("_") | char_("~");
    percent_encoded = char_("%") >> hexdigit >> hexdigit;
    hexdigit = char_('0', '9') | char_('A', 'F');
  }

  qi::rule<iterator, std::vector<param>(), ascii::blank_type> start;
  qi::rule<iterator, param()> kvpair;
  qi::rule<iterator, std::string()> escaped, percent_encoded;
  qi::rule<iterator, char()> unreserved, hexdigit;
};

// parses the oauth authorization header and returns true if the
// parameters were successfully parsed. return false if they were
// not parsed.
bool parse_oauth_authorization(const char *auth_header, std::vector<param> &params) {
  using boost::spirit::ascii::blank;
  typedef const char *iterator_type;
  typedef oauth_authorization_grammar<iterator_type> grammar;

  grammar g;
  iterator_type itr = auth_header;
  if (itr == NULL) { return false; }
  iterator_type end = itr + std::strlen(auth_header);

  bool success = qi::phrase_parse(itr, end, g, blank, params);

  return (success && (itr == end));
}

boost::optional<std::string> normalise_request_parameters(request &req) {
  typedef std::map<std::string, std::string> map_t;
  std::vector<param> params;

  { // add oauth params, except realm
    std::vector<param> auth_params;
    const char *auth_header = req.get_param("HTTP_AUTHORIZATION");
    bool success = parse_oauth_authorization(auth_header, auth_params);
    if (!success) { return boost::none; }

    BOOST_FOREACH(param &p, auth_params) {
      std::string k = http::urldecode(p.k);
      if (k != "realm") {
        params.push_back(param());
        params.back().k.swap(k);
        params.back().v = http::urldecode(p.v);
      }
    }
  }

  { // add HTTP GET parameters
    map_t get_params = http::parse_params(get_query_string(req));
    BOOST_FOREACH(const map_t::value_type &kv, get_params) {
      params.push_back(param());
      params.back().k = http::urldecode(kv.first);
      params.back().v = http::urldecode(kv.second);
    }
  }

  std::sort(params.begin(), params.end());

  std::ostringstream out;
  bool first = true;
  BOOST_FOREACH(const param &p, params) {
    if (first) { first = false; } else { out << "&"; }
    out << http::urlencode(p.k) << "=" << http::urlencode(p.v);
  }

  return out.str();
}

std::string normalise_request_url(request &req) {
  std::ostringstream out;

  out << downcase(scheme(req)) << "://"
      << downcase(authority(req)) << "/"
      << path(req);

  return out.str();
}

std::string request_method(request &req) {
  const char *method = req.get_param("REQUEST_METHOD");
  if (method == NULL) {
    throw http::server_error("Request didn't set $REQUEST_METHOD parameter.");
  }
  return std::string(method);
}

} // anonymous namespace

std::string signature_base_string(request &req) {
  std::ostringstream out;

  out << upcase(request_method(req)) << "&"
      << normalise_request_url(req) << "&"
      << normalise_request_parameters(req);

  return out.str();
}

bool oauth_valid_signature(request &) {
  return false;
}
