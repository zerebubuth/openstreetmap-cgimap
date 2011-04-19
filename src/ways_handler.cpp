#include "ways_handler.hpp"
#include "osm_helpers.hpp"
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

ways_responder::ways_responder(mime::type mt, list<id_t> ids_, pqxx::work &w_)
	: osm_responder(mt, w_), ids(ids_) {

	stringstream query;
	list<id_t>::const_iterator it;
	
	query << "create temporary table tmp_ways as select id from current_ways where id IN (";
	std::copy(ids.begin(), ids.end(), infix_ostream_iterator<id_t>(query, ","));
	query << ") and visible";

	w.exec(query);

	int num_ways = osm_helpers::num_ways(w);

    if ( num_ways != ids.size()) {
        throw http::not_found("One or more of the ways were not found.");
    }
}

ways_responder::~ways_responder() throw() {
}

ways_handler::ways_handler(FCGX_Request &request) 
	: ids(validate_request(request)) {
}

ways_handler::~ways_handler() throw() {
}

std::string 
ways_handler::log_name() const {
	return "ways";
}

responder_ptr_t 
ways_handler::responder(pqxx::work &x) const {
	return responder_ptr_t(new ways_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or 
 * throwing an error if there was no valid list of way ids.
 */
list<id_t>
ways_handler::validate_request(FCGX_Request &request) {
	// check that the REQUEST_METHOD is a GET
	if (fcgi_get_env(request, "REQUEST_METHOD") != "GET") 
		throw http::method_not_allowed("Only the GET method is supported for "
									   "ways requests.");

	string decoded = http::urldecode(get_query_string(request));
	const map<string, string> params = http::parse_params(decoded);
	map<string, string>::const_iterator itr = params.find("ways");

	list <id_t> myids;

	if (itr != params.end()) {
		vector<string> strs;
		al::split(strs, itr->second, al::is_any_of(","));
		try {
			for (vector<string>::iterator itr = strs.begin(); itr != strs.end(); ++itr) { 
				id_t id = lexical_cast<id_t>(*itr);
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

	stringstream msg;
	std::copy(myids.begin(), myids.end(), infix_ostream_iterator<id_t>(msg, ", "));
	logger::message(format("processing ways with ids:  %1%") % msg.str());
    
  return myids;
}
