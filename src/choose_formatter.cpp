#include "choose_formatter.hpp"
#include "output_writer.hpp"
#include "mime_types.hpp"
#include "http.hpp"
#include "fcgi_helpers.hpp"
#include "writer.hpp"
#include "xml_formatter.hpp"
#include "json_writer.hpp"
#include "json_formatter.hpp"

#include <stdexcept>
#include <list>
#include <map>
#include <limits>
#include <boost/optional.hpp>

using std::auto_ptr;
using std::ostringstream;
using std::list;
using std::runtime_error;
using std::map;
using std::numeric_limits;
using boost::optional;
using boost::shared_ptr;

namespace {

class acceptable_types {
public:
	 bool is_acceptable(mime::type) const;
   // note: returns mime::unspecified_type if none were acceptable
	 mime::type most_acceptable_of(const list<mime::type> &available) const;
private:
	 map<mime::type, float> mapping;
};

bool acceptable_types::is_acceptable(mime::type mt) const {
	return mapping.find(mt) != mapping.end();
}

mime::type
acceptable_types::most_acceptable_of(const list<mime::type> &available) const {
	mime::type best = mime::unspecified_type;
	float score = numeric_limits<float>::min();
	for (list<mime::type>::const_iterator itr = available.begin();
			 itr != available.end(); ++itr) {
		map<mime::type, float>::const_iterator jtr = mapping.find(*itr);
		if ((jtr != mapping.end()) && (jtr->second > score)) {
			best = jtr->first;
			score = jtr->second;
		}
	}
	return best;
}

/**
 * figures out the preferred mime type(s) from the Accept headers, mapped to their
 * relative acceptability.
 */
acceptable_types header_mime_type(FCGX_Request &req) {
	return acceptable_types();
}
}

auto_ptr<output_formatter>
choose_formatter(FCGX_Request &request, 
								 responder_ptr_t hptr, 
								 shared_ptr<output_buffer> out,
								 cache<long int, changeset> &changeset_cache) {
	// figure out what, if any, the Accept-able resource mime types are
	acceptable_types types = header_mime_type(request);
	const list<mime::type> types_available = hptr->types_available();
		
	mime::type best_type = hptr->resource_type();
	// check if the handler is capable of supporting an acceptable set of mime types.
	if (best_type != mime::unspecified_type) {
		// check that this doesn't conflict with anything in the Accept header.
		if (!hptr->is_available(best_type) || !types.is_acceptable(best_type)) {
			throw http::not_acceptable(get_request_path(request)); // TODO , types_available);
		}
	} else {
		best_type = types.most_acceptable_of(types_available);
		// if none were acceptable then...
		if (best_type == mime::unspecified_type) {
			throw http::not_acceptable(get_request_path(request)); // TODO , types_available);
		} else if (best_type == mime::any_type) {
			// choose the first of the available types if nothing is preferred.
			best_type = *(hptr->types_available().begin());
		}
		// otherwise we've chosen the most acceptable and available type...
	}
	
	auto_ptr<output_writer> o_writer;
	auto_ptr<output_formatter> o_formatter;

	if (best_type == mime::text_xml) {
	  xml_writer *xwriter = new xml_writer(out, true);
	  o_writer = auto_ptr<output_writer>(xwriter);
	  o_formatter = auto_ptr<output_formatter>(new xml_formatter(*xwriter, changeset_cache));

#ifdef HAVE_YAJL
	} else if (best_type == mime::text_json) {
	  json_writer *jwriter = new json_writer(out, true);
	  o_writer = auto_ptr<output_writer>(jwriter);
	  o_formatter = auto_ptr<output_formatter>(new json_formatter(*jwriter, changeset_cache));
#endif

	} else {
		ostringstream ostr;
		ostr << "Could not create formatter for MIME type `" << mime::to_string(best_type) << "'.";
		throw runtime_error(ostr.str());
	}

	return o_formatter;
}
