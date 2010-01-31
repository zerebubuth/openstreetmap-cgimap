#include "http.hpp"
#include <vector>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

namespace al = boost::algorithm;
using std::string;
using std::map;
using std::vector;
using boost::shared_ptr;

namespace http {

  exception::exception(unsigned int c, const string &h, const string &m) 
    : code_(c), header_(h), message_(m) {}

  exception::~exception() throw() {}

  unsigned int 
  exception::code() const { 
    return code_; 
  }

  const string &
  exception::header() const { 
    return header_; 
  }

  const char *
  exception::what() const throw() { 
    return message_.c_str(); 
  }

  server_error::server_error(const string &message)
    : exception(500, "Internal Server Error", message) {}

  bad_request::bad_request(const string &message)
    : exception(400, "Bad Request", message) {}
  
  method_not_allowed::method_not_allowed(const string &message)
    : exception(405, "Method Not Allowed", message) {}  
  
  map<string, string>
  parse_params(const string &p) {
    // Split the query string into components
    map<string, string> queryKVPairs;
    if (!p.empty()) {
      vector<string> temp;
      al::split(temp, p, al::is_any_of ("&"));
      
      BOOST_FOREACH(const string &kvPair, temp) {
	vector<string> kvTemp;
	al::split(kvTemp, kvPair, al::is_any_of ("="));
	// ASSERT( kvTemp.size() == 2 );
	if (kvTemp.size() == 2) {
	  queryKVPairs[kvTemp[0]] = kvTemp[1];
	}
      }
    }
    return queryKVPairs;
  }

  shared_ptr<encoding>
  choose_encoding(const string &accept_encoding) {
    vector<string> encodings;
     
    al::split(encodings, accept_encoding, al::is_any_of(","));

    float identity_quality = 0.001;
    float deflate_quality = 0.000;
    float gzip_quality = 0.000;

    BOOST_FOREACH(const string &encoding, encodings) {
      boost::smatch what;
      string name;
      float quality;

      if (boost::regex_match(encoding, what, boost::regex("\\s*([^()<>@,;:\\\\\"/[\\]\\\\?={} \\t]+)\\s*;\\s*q\\s*=(\\d+(\\.\\d+)?)\\s*"))) {
        name = what[1];
        quality = std::atof(string(what[2]).c_str());
      }
      else if (boost::regex_match(encoding, what, boost::regex("\\s*([^()<>@,;:\\\\\"/[\\]\\\\?={} \\t]+)\\s*"))) {
        name = what[1];
        quality = 1.0;
      }
      else {
        name = "";
        quality = 0.0;
      }

      if (al::iequals(name, "identity")) {
        identity_quality = quality;
      }
      else if (al::iequals(name, "deflate")) {
        deflate_quality = quality;
      }
      else if (al::iequals(name, "gzip")) {
        gzip_quality = quality;
      }
    }

#ifdef ENABLE_DEFLATE
    if (deflate_quality >= gzip_quality && deflate_quality >= identity_quality) {
      return shared_ptr<deflate>(new deflate());
    }
    else
#endif
    if (gzip_quality >= identity_quality) {
      return shared_ptr<gzip>(new gzip());
    }
    else {
      return shared_ptr<identity>(new identity());
    }
  }
}
