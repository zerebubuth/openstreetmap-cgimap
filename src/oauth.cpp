#include "cgimap/config.hpp"
#include "cgimap/oauth.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"

#include <algorithm>
#include <vector>
#include <sstream>

#include <boost/foreach.hpp>
#ifdef HAVE_BOOST_LOCALE
#include <boost/locale.hpp>
#else
#include <ctype.h> // for toupper / tolower
#endif /* HAVE_BOOST_LOCALE */

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cryptopp/hmac.h>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>

namespace bai = boost::archive::iterators;
namespace pt = boost::posix_time;

namespace {

#ifdef HAVE_BOOST_LOCALE
const std::locale ascii_locale = boost::locale::generator()("C.UTF-8");

std::string downcase(const std::string &s) {
  return boost::locale::to_lower(s, ascii_locale);
}

std::string upcase(const std::string &s) {
  return boost::locale::to_upper(s, ascii_locale);
}

#else /* HAVE_BOOST_LOCALE */
// if we don't have boost::locale available, then instead of trying
// to interface to ICU directly, just fall back to only supporting
// ASCII...
std::string downcase(const std::string &s) {
  std::string rv(s);
  std::transform(rv.begin(), rv.end(), rv.begin(), ::tolower);
  return rv;
}

std::string upcase(const std::string &s) {
  std::string rv(s);
  std::transform(rv.begin(), rv.end(), rv.begin(), ::toupper);
  return rv;
}
#endif /* HAVE_BOOST_LOCALE */

std::string scheme(request &req) {
  const char *https = req.get_param("HTTPS");

  if (https == NULL) {
    return "http";

  } else {
    return "https";
  }
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

bool begins_with(const std::string &str, const std::string &prefix) {
  return (str.size() >= prefix.size()) &&
    std::equal(prefix.begin(), prefix.end(), str.begin());
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
    key = qi::raw[+(unreserved | percent_encoded)];
    escaped = qi::raw[*(unreserved | percent_encoded)];
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
  qi::rule<iterator, std::string()> key, unreserved, escaped, percent_encoded,
    quoted_string;
  qi::rule<iterator, char()> hexdigit, qdtext, quoted_pair;
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

std::string urlnormalise(const std::string &str) {
  return http::urlencode(http::urldecode(str));
}

bool get_all_request_parameters(request &req, std::vector<param> &params) {
  typedef std::vector<std::pair<std::string, std::string> > params_t;

  { // add oauth params, except realm
    std::vector<param> auth_params;
    const char *auth_header = req.get_param("HTTP_AUTHORIZATION");
    bool success = parse_oauth_authorization(auth_header, auth_params);

    if (success) {
      BOOST_FOREACH(param &p, auth_params) {
        std::string k = http::urldecode(p.k);
        params.push_back(param());
        params.back().k = urlnormalise(p.k);
        params.back().v = urlnormalise(p.v);
      }
    }
  }

  { // add HTTP GET parameters
    params_t get_params = http::parse_params(get_query_string(req));
    BOOST_FOREACH(const params_t::value_type &kv, get_params) {
      params.push_back(param());
      params.back().k = urlnormalise(kv.first);
      params.back().v = urlnormalise(kv.second);
    }
  }

  std::sort(params.begin(), params.end());

  // check for duplicate protocol parameters
  std::string last_key;
  BOOST_FOREACH(const param &p, params) {
    std::string k = http::urldecode(p.k);
    if (begins_with(k, "oauth_") && (p.k == last_key)) {
      return false;
    }
    last_key = k;
  }

  return true;
}

boost::optional<std::string> find_auth_header_value(const std::string &key, request &req) {
  std::vector<param> params;
  if (!get_all_request_parameters(req, params)) {
    return boost::none;
  }

  for (std::vector<param>::const_iterator itr = params.begin();
       itr != params.end(); ++itr) {
    if (http::urldecode(itr->k) == key) {
      return http::urldecode(itr->v);
    }
  }

  return boost::none;
}

boost::optional<std::string> signature_method(request &req) {
  return find_auth_header_value("oauth_signature_method", req);
}

boost::optional<std::string> get_consumer_key(request &req) {
  return find_auth_header_value("oauth_consumer_key", req);
}

boost::optional<std::string> get_token_id(request &req) {
  return find_auth_header_value("oauth_token", req);
}

} // anonymous namespace

namespace oauth {

namespace detail {

std::string hmac_sha1(const std::string &key, const std::string &text) {
  std::string hash;

  CryptoPP::HMAC<CryptoPP::SHA1> hmac(
    reinterpret_cast<const unsigned char *>(key.c_str()),
    key.size());

  CryptoPP::StringSource ss(
    text, true,
    new CryptoPP::HashFilter(
      hmac,
      new CryptoPP::StringSink(
        hash
        )));

  return hash;
}

std::string base64_encode(const std::string &str) {
  typedef
    bai::base64_from_binary<
      bai::transform_width<
        std::string::const_iterator,
        6,
        8
      >
    >
    base64_encode;

  std::ostringstream ostr;
  std::copy(base64_encode(str.begin()),
            base64_encode(str.end()),
            std::ostream_iterator<char>(ostr));

  // add '=' for padding
  size_t dangling_bytes = str.size() % 3;
  if (dangling_bytes == 1) {
    ostr << "==";

  } else if (dangling_bytes == 2) {
    ostr << "=";
  }

  return ostr.str();
}

boost::optional<std::string> normalise_request_parameters(request &req) {
  std::vector<param> params;

  if (!get_all_request_parameters(req, params)) {
    return boost::none;
  }

  std::ostringstream out;
  bool first = true;
  BOOST_FOREACH(const param &p, params) {
    if ((p.k != "realm") && (p.k != "oauth_signature")) {
      if (first) { first = false; } else { out << "&"; }
      out << p.k << "=" << p.v;
    }
  }

  return out.str();
}

std::string normalise_request_url(request &req) {
  std::ostringstream out;

  out << downcase(scheme(req)) << "://"
      << downcase(authority(req));
  std::string p = path(req);
  if (p.size() > 0 && p[0] != '/') {
    out << "/";
  }
  out << p;

  return out.str();
}

boost::optional<std::string> signature_base_string(request &req) {
  std::ostringstream out;

  boost::optional<std::string> request_params =
    normalise_request_parameters(req);

  if (!request_params) {
    return boost::none;
  }

  out << http::urlencode(upcase(request_method(req))) << "&"
      << http::urlencode(normalise_request_url(req)) << "&"
      << http::urlencode(*request_params);

  return out.str();
}

boost::optional<std::string> hashed_signature(request &req, secret_store &store) {
  boost::optional<std::string> method = signature_method(req);
  if (!method || ((*method != "HMAC-SHA1") && (*method != "PLAINTEXT"))) {
    return boost::none;
  }

  boost::optional<std::string>
    sig_base_string = signature_base_string(req);
  if (!sig_base_string) { return boost::none; }

  boost::optional<std::string>
    consumer_key = get_consumer_key(req),
    token_id = get_token_id(req);

  if (!consumer_key) { return boost::none; }
  if (!token_id) { return boost::none; }

  boost::optional<std::string>
    consumer_secret = store.consumer_secret(*consumer_key),
    token_secret = store.token_secret(*token_id);

  if (!consumer_secret) { return boost::none; }
  if (!token_secret) { return boost::none; }

  std::ostringstream key;
  key << http::urlencode(*consumer_secret) << "&"
      << http::urlencode(*token_secret);

  std::string hash;
  if (*method == "HMAC-SHA1") {
    try {
      hash = base64_encode(hmac_sha1(key.str(), *sig_base_string));

    } catch (const CryptoPP::Exception &e) {
      // TODO: log error!
      return boost::none;
    }

  } else { // PLAINTEXT
    hash = key.str();
  }

  return hash;
}

} // namespace detail

validity::validity is_valid_signature(
  request &req, secret_store &store,
  nonce_store &nonces, token_store &tokens) {

  boost::optional<std::string>
    provided_signature = find_auth_header_value("oauth_signature", req);
  if (!provided_signature) {
    return validity::not_signed();
  }

  boost::optional<std::string>
    calculated_signature = detail::hashed_signature(req, store);
  if (!calculated_signature) {
    // 400 - bad request, according to section 3.2.
    return validity::bad_request();
  }

  // check the signatures are identical
  if (*calculated_signature != *provided_signature) {
    // 401 - unauthorized, according to section 3.2.
    return validity::unauthorized(
      "Calculated signature differs from provided.");
  }

  boost::optional<std::string>
    token = find_auth_header_value("oauth_token", req);
  if (!token) {
    // 401 - unauthorized, according to section 3.2.
    return validity::unauthorized(
      "No oauth_token Authorization parameter found.");
  }

  if (signature_method(req) != std::string("PLAINTEXT")) {
    // verify nonce/timestamp/token hasn't been used before
    boost::optional<std::string>
      nonce = find_auth_header_value("oauth_nonce", req),
      timestamp_str = find_auth_header_value("oauth_timestamp", req);

    if (!nonce || !timestamp_str) {
      // 401 - unauthorized, according to section 3.2.
      return validity::unauthorized(
        "Both nonce and timestamp must be provided.");
    }

    std::uint64_t timestamp = 0;
    {
      std::istringstream stream(*timestamp_str);

      // the timestamp MUST be an integer (section 8).
      if (!(stream >> timestamp)) {
        return validity::bad_request();
      }

      // check that the time isn't too far in the past
      if (req.get_current_time() - pt::from_time_t(timestamp) > pt::hours(24)) {
        return validity::unauthorized(
          "Timestamp is too far in the past.");
      }
    }

    // check that the nonce hasn't been used before.
    if (!nonces.use_nonce(*nonce, timestamp)) {
      // 401 - unauthorized, according to section 3.2.
      return validity::unauthorized(
        "Nonce has been used before.");
    }
  }

  // verify that the token allows api access.
  if (!tokens.allow_read_api(*token)) {
    // the signature is okay, but the token isn't authorized for API access,
    // so probably best to return a 401 - unauthorized.
    return validity::unauthorized(
      "The user token does not allow read API access.");
  }

  boost::optional<std::string>
    version = find_auth_header_value("oauth_version", req);
  if (version && (*version != "1.0")) {
    // ??? probably a 400?
    return validity::bad_request();
  }

  return validity::copacetic(*token);
}

store::~store() {
}

} // namespace oauth
