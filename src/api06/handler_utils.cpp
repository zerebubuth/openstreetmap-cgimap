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
using std::pair;
using boost::lexical_cast;
using boost::bad_lexical_cast;
namespace al = boost::algorithm;

namespace {
struct first_equals {
  first_equals(const std::string &k) : m_key(k) {}
  bool operator()(const pair<string, string> &v) const {
    return v.first == m_key;
  }
  string m_key;
};
} // anonymous namespace

namespace api06 {

vector<osm_nwr_id_t> parse_id_list_params(request &req, const string &param_name) {
  string decoded = http::urldecode(get_query_string(req));
  const vector<pair<string, string> > params = http::parse_params(decoded);
  vector<pair<string, string> >::const_iterator itr =
    std::find_if(params.begin(), params.end(), first_equals(param_name));

  vector<osm_nwr_id_t> myids;

  if (itr != params.end()) {
    // just check for blank string, which shouldn't be converted.
    if (!itr->second.empty()) {
      vector<string> strs;
      al::split(strs, itr->second, al::is_any_of(","));
      try {
        for (vector<string>::iterator itr = strs.begin(); itr != strs.end();
             ++itr) {
          // parse as a 64-bit signed int to catch negative numbers. postgres
          // bigint is int64_t anyway, so this doesn't truncate the allowed
          // range.
          int64_t id = lexical_cast<int64_t>(*itr);
          // note: osm_nwr_id_t is currently uint64_t, so int64_t cannot
          // overflow it in the positive direction. this would need to change
          // if the types changed.
          if (id > 0) {
            myids.push_back(osm_nwr_id_t(id));

          } else {
            // simulate rails_port behaviour - things that we can't parse are
            // treated as zero.
            myids.push_back(0);
          }
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

  // ensure list of IDs is unique
  std::sort(myids.begin(), myids.end());
  vector<osm_nwr_id_t>::iterator new_end = std::unique(myids.begin(), myids.end());
  myids.erase(new_end, myids.end());

  return myids;
}
}
