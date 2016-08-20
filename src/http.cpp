#include "cgimap/config.hpp"
#include "cgimap/http.hpp"
#include <vector>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <iterator> // for distance
#include <cctype>   // for toupper, isxdigit
#include <sstream>

namespace al = boost::algorithm;
using std::string;
using std::vector;
using std::pair;
using boost::shared_ptr;

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

exception::exception(unsigned int c, const string &h, const string &m)
    : code_(c), header_(h), message_(m) {}

exception::~exception() throw() {}

unsigned int exception::code() const { return code_; }

const string &exception::header() const { return header_; }

const char *exception::what() const throw() { return message_.c_str(); }

server_error::server_error(const string &message)
    : exception(500, "Internal Server Error", message) {}

bad_request::bad_request(const string &message)
    : exception(400, "Bad Request", message) {}

not_found::not_found(const string &uri) : exception(404, "Not Found", uri) {}

not_acceptable::not_acceptable(const string &message)
    : exception(406, "Not Acceptable", message) {}

bandwidth_limit_exceeded::bandwidth_limit_exceeded(const string &message)
    : exception(509, "Bandwidth Limit Exceeded", message) {}

gone::gone() : exception(410, "Gone", "") {}

unauthorized::unauthorized(const std::string &message)
  : exception(401, "Unauthorized", message) {}

string urldecode(const string &s) { return form_urldecode(s); }

string urlencode(const string &s) {
  static const char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  std::ostringstream ostr;

  BOOST_FOREACH(char c, s) {
    if (((c >= 'a') && (c <= 'z')) ||
        ((c >= 'A') && (c <= 'Z')) ||
        ((c >= '0') && (c <= '9')) ||
        (c == '-') ||
        (c == '.') ||
        (c == '_') ||
        (c == '~')) {
      ostr << c;

    } else {
      unsigned char idx = (unsigned char)(c);
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

    BOOST_FOREACH(const string &kvPair, temp) {
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

shared_ptr<encoding> choose_encoding(const string &accept_encoding) {
  vector<string> encodings;

  al::split(encodings, accept_encoding, al::is_any_of(","));

  float identity_quality = 0.001;
  float deflate_quality = 0.000;
  float gzip_quality = 0.000;

  BOOST_FOREACH(const string &encoding, encodings) {
    boost::smatch what;
    string name;
    float quality;

    if (boost::regex_match(
            encoding, what,
            boost::regex("\\s*([^()<>@,;:\\\\\"/[\\]\\\\?={} "
                         "\\t]+)\\s*;\\s*q\\s*=(\\d+(\\.\\d+)?)\\s*"))) {
      name = what[1];
      quality = std::atof(string(what[2]).c_str());
    } else if (boost::regex_match(
                   encoding, what,
                   boost::regex(
                       "\\s*([^()<>@,;:\\\\\"/[\\]\\\\?={} \\t]+)\\s*"))) {
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
    } else if (al::iequals(name, "*")) {
      if (identity_quality == 0.000)
        identity_quality = quality;
      if (deflate_quality == 0.000)
        deflate_quality = quality;
      if (gzip_quality == 0.001)
        gzip_quality = quality;
    }
  }

#ifdef HAVE_LIBZ
#ifdef ENABLE_DEFLATE
  if (deflate_quality > 0.0 && deflate_quality >= gzip_quality &&
      deflate_quality >= identity_quality) {
    return shared_ptr<deflate>(new deflate());
  } else
#endif /* ENABLE_DEFLATE */
      if (gzip_quality > 0.0 && gzip_quality >= identity_quality) {
    return shared_ptr<gzip>(new gzip());
  }
#endif /* HAVE_LIBZ */
  else if (identity_quality > 0.0) {
    return shared_ptr<identity>(new identity());
  } else {
    throw http::not_acceptable("No acceptable content encoding found. Only "
                               "identity and gzip are supported.");
  }
}
}
