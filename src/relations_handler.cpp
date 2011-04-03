#include "relations_handler.hpp"
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

relations_responder::relations_responder(mime::type mt, list<id_t> ids_, pqxx::work &w_)
	: osm_responder(mt, w_, false, false, true), ids(ids_) {

	stringstream query;
	list<id_t>::const_iterator it;
	
	query << "create temporary table tmp_relations as select id from current_relations where id IN (";
	std::copy(ids.begin(), ids.end(), infix_ostream_iterator<id_t>(query, ","));
	query << ") and visible";

	w.exec(query);

	int num_relations = osm_helpers::num_relations(w);

    if ( num_relations != ids.size()) {
        throw http::not_found("One or more of the relations were not found.");			
    }
}

relations_responder::~relations_responder() throw() {
}

relations_handler::relations_handler(FCGX_Request &request) 
	: ids(validate_request(request)) {
}

relations_handler::~relations_handler() throw() {
}

std::string 
relations_handler::log_name() const {
	return "relations";
}

responder_ptr_t 
relations_handler::responder(pqxx::work &x) const {
	return responder_ptr_t(new relations_responder(mime_type, ids, x));
}

/**
 * Validates an FCGI request, returning the valid list of ids or 
 * throwing an error if there was no valid list of node ids.
 */
list<id_t>
relations_handler::validate_request(FCGX_Request &request) {
	// check that the REQUEST_METHOD is a GET
	if (fcgi_get_env(request, "REQUEST_METHOD") != "GET") 
		throw http::method_not_allowed("Only the GET method is supported for "
									   "relations requests.");

	string decoded = http::urldecode(get_query_string(request));
	const map<string, string> params = http::parse_params(decoded);
	map<string, string>::const_iterator itr = params.find("relations");

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
			throw http::bad_request("The parameter relations is required, and must be "
									"of the form relations=id[,id[,id...]].");
		}
	}

	stringstream msg;
	std::copy(myids.begin(), myids.end(), infix_ostream_iterator<id_t>(msg, ", "));
	logger::message(format("processing relations with ids:  %1%") % msg.str());
    
  return myids;
}
