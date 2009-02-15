#include "http.hpp"
#include <vector>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

namespace al = boost::algorithm;
using std::string;
using std::map;
using std::vector;

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
}

