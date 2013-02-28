#include "api06/relations_handler.hpp"
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

relations_responder::relations_responder(mime::type mt, list<osm_id_t> ids_, data_selection &s_)
	: osm_responder(mt, s_), ids(ids_) {

	sel.select_relations(ids_);

	int num_relations = sel.num_relations();

    if ( num_relations != ids.size()) {
        throw http::not_found("One or more of the relations were not found.");			
    }
}

relations_responder::~relations_responder() {
}

relations_handler::relations_handler(FCGX_Request &request) 
	: ids(validate_request(request)) {
}

relations_handler::~relations_handler() {
}

std::string 
relations_handler::log_name() const {
  stringstream msg;
  msg << "relations?relations=";
  std::copy(ids.begin(), ids.end(), infix_ostream_iterator<osm_id_t>(msg, ", "));
  return msg.str();
}

responder_ptr_t 
relations_handler::responder(data_selection &x) const {
	return responder_ptr_t(new relations_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or 
 * throwing an error if there was no valid list of node ids.
 */
list<osm_id_t>
relations_handler::validate_request(FCGX_Request &request) {
	// check that the REQUEST_METHOD is a GET
	if (fcgi_get_env(request, "REQUEST_METHOD") != "GET") 
		throw http::method_not_allowed("Only the GET method is supported for "
									   "relations requests.");

	string decoded = http::urldecode(get_query_string(request));
	const map<string, string> params = http::parse_params(decoded);
	map<string, string>::const_iterator itr = params.find("relations");

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
			throw http::bad_request("The parameter relations is required, and must be "
									"of the form relations=id[,id[,id...]].");
		}
	}

    if (myids.size() < 1) {
        throw http::bad_request("The parameter relations is required, and must be "
            "of the form relations=id[,id[,id...]].");
    }

  return myids;
}

} // namespace api06

