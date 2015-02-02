#include "cgimap/api06/handler_utils.hpp"

#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include <map>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

using std::list;
using std::string;
using std::map;
using std::vector;
using boost::lexical_cast;
using boost::bad_lexical_cast;
namespace al = boost::algorithm;

namespace api06 {

list<osm_id_t> parse_id_list_params(request &req, const string &param_name) {
  string decoded = http::urldecode(get_query_string(req));
  const map<string, string> params = http::parse_params(decoded);
  map<string, string>::const_iterator itr = params.find(param_name);

  list<osm_id_t> myids;

  if (itr != params.end()) {
    // just check for blank string, which shouldn't be converted.
    if (!itr->second.empty()) {
      vector<string> strs;
      al::split(strs, itr->second, al::is_any_of(","));
      try {
        for (vector<string>::iterator itr = strs.begin(); itr != strs.end();
             ++itr) {
          osm_id_t id = lexical_cast<osm_id_t>(*itr);
          myids.push_back(id);
        }
      } catch (const bad_lexical_cast &) {
        // note: this is pretty silly, and may change when the rails_port
        // changes to more sensible behaviour... but for the moment we emulate
        // the ruby behaviour; which is that something non-numeric results in a
        // to_i result of zero.
        myids.push_back(0);
      }
    }
  }

  return myids;
}
}
