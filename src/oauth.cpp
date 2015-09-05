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

  return host;
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

    start = lit("OAuth") >> (lexeme[kvpair] % char_(','));

    // looks like realm is special, see the OAuth spec for details
    // http://oauth.net/core/1.0a/#rfc.section.5.4.1
    kvpair
      = (ascii::string("realm") >> lit("=\"") > quoted_string > lit("\""))
      | (key >> lit("=\"") > escaped > lit("\""));

    // definitions from http://oauth.net/core/1.0a/#rfc.section.5.4.1
    key = +(unreserved | percent_encoded);
    escaped = *(unreserved | percent_encoded);
    unreserved = char_('a', 'z') | char_('A', 'Z') | char_('0', '9') | char_("-") |
      char_(".") | char_("_") | char_("~");
    percent_encoded = char_("%") > hexdigit > hexdigit;
    hexdigit = char_('0', '9') | char_('A', 'F');

    // these are from RFC2616 section 2.2.
    quoted_string = *(qdtext | quoted_pair);
    qdtext = char_(32, 126) - char_('\\') - char_('"');
    quoted_pair = lit("\\") > char_(0, 127);

    /* uncomment below to get debugging information from the
     * spirit/qi rules. *
    start.name("start");
    kvpair.name("kvpair");
    key.name("key");
    escaped.name("escaped");
    unreserved.name("unreserved");
    percent_encoded.name("percent_encoded");
    hexdigit.name("hexdigit");
    quoted_string.name("quoted_string");
    qdtext.name("qdtext");
    quoted_pair.name("quoted_pair");

    using qi::debug;
    debug(start);
    debug(kvpair);
    debug(key);
    debug(escaped);
    debug(unreserved);
    debug(percent_encoded);
    debug(quoted_string);
    debug(qdtext);
    debug(quoted_pair);
    */
  }

  qi::rule<iterator, std::vector<param>(), ascii::blank_type> start;
  qi::rule<iterator, param()> kvpair;
  qi::rule<iterator, std::string()> key, escaped, percent_encoded, quoted_string;
  qi::rule<iterator, char()> unreserved, hexdigit, qdtext, quoted_pair;
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

std::string request_method(request &req) {
  const char *method = req.get_param("REQUEST_METHOD");
  if (method == NULL) {
    throw http::server_error("Request didn't set $REQUEST_METHOD parameter.");
  }
  return std::string(method);
}

std::string escape(const std::string &str) {
  static const char *hexdigits = "0123456789ABCDEF";

  std::ostringstream ostr;
  BOOST_FOREACH(char c, str) {
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '-') ||
        (c == '.') ||
        (c == '_') ||
        (c == '~')) {
      ostr << c;

    } else {
      unsigned char uc = (unsigned char)c;
      ostr << "%" << hexdigits[uc >> 4] << hexdigits[uc & 0xf];
    }
  }

  return ostr.str();
}

} // anonymous namespace

namespace oauth {

namespace detail {

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
      if ((k != "realm") && (k != "oauth_signature")) {
        params.push_back(param());
        params.back().k.swap(k);
        params.back().v = http::urldecode(p.v);
      }
    }
  }

  { // add HTTP GET parameters
    map_t get_params = http::parse_params(get_query_string(req));
    BOOST_FOREACH(const map_t::value_type &kv, get_params) {
      if ((kv.first != "realm") && (kv.first != "oauth_signature")) {
        params.push_back(param());
        params.back().k = http::urldecode(kv.first);
        params.back().v = http::urldecode(kv.second);
      }
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

boost::optional<std::string> signature_base_string(request &req) {
  std::ostringstream out;

  boost::optional<std::string> request_params =
    normalise_request_parameters(req);

  if (!request_params) {
    return boost::none;
  }

  out << escape(upcase(request_method(req))) << "&"
      << escape(normalise_request_url(req)) << "&"
      << escape(*request_params);

  return out.str();
}

} // namespace detail

bool is_valid_signature(request &) {
  return false;
}

} // namespace oauth
