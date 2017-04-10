#include "cgimap/config.hpp"
#include "cgimap/choose_formatter.hpp"
#include "cgimap/output_writer.hpp"
#include "cgimap/mime_types.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/xml_writer.hpp"
#include "cgimap/xml_formatter.hpp"
#include "cgimap/json_writer.hpp"
#include "cgimap/json_formatter.hpp"
#include "cgimap/logger.hpp"

#include <stdexcept>
#include <list>
#include <map>
#include <limits>
#include <boost/optional.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_statement.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

using std::auto_ptr;
using std::ostringstream;
using std::list;
using std::runtime_error;
using std::map;
using std::numeric_limits;
using std::make_pair;
using boost::optional;
using boost::shared_ptr;
using boost::lexical_cast;
using std::string;
using std::vector;
using std::pair;
using boost::format;

namespace {

class acceptable_types {
public:
  acceptable_types(const std::string &accept_header);
  bool is_acceptable(mime::type) const;
  // note: returns mime::unspecified_type if none were acceptable
  mime::type most_acceptable_of(const list<mime::type> &available) const;

private:
  map<mime::type, float> mapping;
};

struct media_range {
  typedef map<string, string> param_t;
  string mime_type;
  param_t params;
};
}

// clang-format off
BOOST_FUSION_ADAPT_STRUCT(
  media_range,
  (string, mime_type)
  (media_range::param_t, params)
  )
// clang-format on

namespace {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename iterator>
struct http_accept_grammar
    : qi::grammar<iterator, vector<media_range>(), ascii::blank_type> {
  http_accept_grammar() : http_accept_grammar::base_type(start) {
    using qi::lit;
    using qi::char_;
    using qi::_val;
    using qi::_1;
    using qi::_2;
    using qi::lexeme;
    using qi::raw;

    // RFC2616 definition of a token
    token %= +(char_(33, 126) -
               (char_("(") | char_(")") | char_("<") | char_(">") | char_("@") |
                char_(",") | char_(";") | char_(":") | char_("\\") |
                char_("\"") | char_("/") | char_("[") | char_("]") |
                char_("?") | char_("=") | char_("{") | char_("}")));
    // RFC2616 definition of a quoted string
    quoted_string %= lit("\"") >>
                     *((char_(32, 126) - char_("\"")) | (lit("\\") >> char_)) >>
                     lit("\"");

    // the string here can be either a token/token pair (where token can include
    // '*') or a single '*' character.
    // TODO: this will accept '*/something', but that is an invalid mime type
    // and should be rejected.
    mime_type %= raw[(token >> lit("/") >> token) | lit("*")];
    param %= token >> '=' >> (token | quoted_string);
    range %= mime_type >> *(';' >> param);
    start %= range % ',';
  }

  qi::rule<iterator, string()> token, quoted_string, mime_type;
  qi::rule<iterator, pair<string, string>(), ascii::blank_type> param;
  qi::rule<iterator, media_range(), ascii::blank_type> range;
  qi::rule<iterator, vector<media_range>(), ascii::blank_type> start;
};
/*
      = lit("* / *")      [_val = mime::any_type]
      | lit("text/xml") [_val = mime::text_xml]
#ifdef HAVE_YAJL
      | lit("text/json")[_val = mime::text_json]
#endif
      ;
*/

acceptable_types::acceptable_types(const std::string &accept_header) {
  using boost::spirit::ascii::blank;
  typedef std::string::const_iterator iterator_type;
  typedef http_accept_grammar<iterator_type> grammar;

  vector<media_range> ranges;
  grammar g;
  iterator_type itr = accept_header.begin();
  iterator_type end = accept_header.end();
  bool status = phrase_parse(itr, end, g, blank, ranges);

  if (status && (itr == end)) {
    BOOST_FOREACH(media_range range, ranges) {
      // figure out the mime::type from the string.
      mime::type mime_type = mime::parse_from(range.mime_type);
      if (mime_type == mime::unspecified_type) {
        // if it's unknown then skip this type...
        continue;
      }

      // figure out the quality
      media_range::param_t::iterator q_itr = range.params.find("q");
      // default quality parameter is 1
      float quality = 1.0;
      if (q_itr != range.params.end()) {
        quality = lexical_cast<float>(q_itr->second);
      }

      mapping.insert(make_pair(mime_type, quality));
    }

  } else {
    logger::message(format("Failed to parse accept header '%1%'") %
                    accept_header);
    throw http::bad_request("Accept header could not be parsed.");
  }
}

bool acceptable_types::is_acceptable(mime::type mt) const {
  return mapping.find(mt) != mapping.end();
}

mime::type
acceptable_types::most_acceptable_of(const list<mime::type> &available) const {
  mime::type best = mime::unspecified_type;
  float score = numeric_limits<float>::min();
  BOOST_FOREACH(mime::type type, available) {
    map<mime::type, float>::const_iterator itr = mapping.find(type);
    if ((itr != mapping.end()) && (itr->second > score)) {
      best = itr->first;
      score = itr->second;
    }
  }

  // todo: check the partial wildcards.

  // also check the full wildcard.
  if (available.size() > 0) {
    map<mime::type, float>::const_iterator itr = mapping.find(mime::any_type);
    if ((itr != mapping.end()) && (itr->second > score)) {
      best = available.front();
    }
  }

  return best;
}

/**
 * figures out the preferred mime type(s) from the Accept headers, mapped to
 * their relative acceptability.
 */
acceptable_types header_mime_type(request &req) {
  // need to look at HTTP_ACCEPT request environment
  string accept_header = fcgi_get_env(req, "HTTP_ACCEPT", "*/*");
  return acceptable_types(accept_header);
}
}

mime::type choose_best_mime_type(request &req, responder_ptr_t hptr) {
  // figure out what, if any, the Accept-able resource mime types are
  acceptable_types types = header_mime_type(req);
  const list<mime::type> types_available = hptr->types_available();

  mime::type best_type = hptr->resource_type();
  // check if the handler is capable of supporting an acceptable set of mime
  // types.
  if (best_type != mime::unspecified_type) {
    // check that this doesn't conflict with anything in the Accept header.
    if (!hptr->is_available(best_type) || !types.is_acceptable(best_type)) {
      // TODO: include types_available in response.
      throw http::not_acceptable(get_request_path(req));
    }
  } else {
    best_type = types.most_acceptable_of(types_available);
    // if none were acceptable then...
    if (best_type == mime::unspecified_type) {
      // TODO: include types_available in response.
      throw http::not_acceptable(get_request_path(req));
    } else if (best_type == mime::any_type) {
      // choose the first of the available types if nothing is preferred.
      best_type = *(hptr->types_available().begin());
    }
    // otherwise we've chosen the most acceptable and available type...
  }

  return best_type;
}

shared_ptr<output_formatter> create_formatter(request &req,
                                              mime::type best_type,
                                              shared_ptr<output_buffer> out) {
  shared_ptr<output_formatter> o_formatter;

  if (best_type == mime::text_xml) {
    xml_writer *xwriter = new xml_writer(out, true);
    o_formatter = shared_ptr<output_formatter>(new xml_formatter(xwriter));

#ifdef HAVE_YAJL
  } else if (best_type == mime::text_json) {
    json_writer *jwriter = new json_writer(out, true);
    o_formatter = shared_ptr<output_formatter>(new json_formatter(jwriter));
#endif

  } else {
    ostringstream ostr;
    ostr << "Could not create formatter for MIME type `"
         << mime::to_string(best_type) << "'.";
    throw runtime_error(ostr.str());
  }

  return o_formatter;
}
