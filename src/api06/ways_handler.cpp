#include "api06/ways_handler.hpp"
#include "http.hpp"
#include "logger.hpp"
#include "infix_ostream_iterator.hpp"

#include <sstream>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace al = boost::algorithm;

using std::stringstream;
using std::list;
using std::vector;
using std::string;
using std::map;
using boost::format;
using boost::lexical_cast;
using boost::bad_lexical_cast;

namespace api06 {

ways_responder::ways_responder(mime::type mt, list<osm_id_t> ids_, data_selection &w_)
  : osm_responder(mt, w_), ids(ids_) {

  if ( sel.select_ways(ids_) != ids.size()) {
    throw http::not_found("One or more of the ways were not found.");
  }
}

ways_responder::~ways_responder() {
}

ways_handler::ways_handler(FCGX_Request &request) 
	: ids(validate_request(request)) {
}

ways_handler::~ways_handler() {
}

std::string 
ways_handler::log_name() const {
  stringstream msg;
  msg << "ways?ways=";
  std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(msg, ", "));
  return msg.str();
}

responder_ptr_t 
ways_handler::responder(data_selection &x) const {
	return responder_ptr_t(new ways_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or 
 * throwing an error if there was no valid list of way ids.
 */
list<osm_id_t>
ways_handler::validate_request(FCGX_Request &request) {
	string decoded = http::urldecode(get_query_string(request));
	const map<string, string> params = http::parse_params(decoded);
	map<string, string>::const_iterator itr = params.find("ways");

	list <osm_id_t> myids;

	if (itr != params.end()) {
		vector<string> strs;
		al::split(strs, itr->second, al::is_any_of(","));
		try {
			for (vector<string>::iterator itr = strs.begin(); itr != strs.end(); ++itr) { 
				osm_id_t id = lexical_cast<osm_id_t>(*itr);
				myids.push_back(id);
			}
		} catch (const bad_lexical_cast &) {
			throw http::bad_request("The parameter ways is required, and must be "
									"of the form ways=id[,id[,id...]].");
		}
	}

    if (myids.size() < 1) {
        throw http::bad_request("The parameter ways is required, and must be "
            "of the form ways=id[,id[,id...]].");
    }
    
  return myids;
}

} // namespace api06

