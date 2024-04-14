/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2023 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/choose_formatter.hpp"
#include "cgimap/output_writer.hpp"
#include "cgimap/mime_types.hpp"
#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/xml_writer.hpp"
#include "cgimap/xml_formatter.hpp"
#if HAVE_YAJL
#include "cgimap/json_writer.hpp"
#include "cgimap/json_formatter.hpp"
#endif
#include "cgimap/text_writer.hpp"
#include "cgimap/text_formatter.hpp"
#include "cgimap/logger.hpp"

#include <stdexcept>
#include <list>
#include <map>
#include <limits>
#include <optional>
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

#include <fmt/core.h>

using std::list;
using std::map;
using std::make_pair;

using std::string;
using std::pair;


namespace {

class acceptable_types {
public:
  explicit acceptable_types(const std::string &accept_header);
  bool is_acceptable(mime::type) const;
  // note: returns mime::type::unspecified_type if none were acceptable
  mime::type most_acceptable_of(const std::vector<mime::type> &available) const;

private:
  map<mime::type, float> mapping;
};

struct media_range {
  using param_t = map<string, string>;
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
    : qi::grammar<iterator, std::vector<media_range>(), ascii::blank_type> {
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
  qi::rule<iterator, std::vector<media_range>(), ascii::blank_type> start;
};
/*
      = lit("* / *")      [_val = mime::any_type]
      | lit("text/xml") [_val = mime::type::application_xml]
      | lit("application/xml") [_val = mime::type::application_xml]
#if HAVE_YAJL
      | lit("application/json")[_val = mime::type::application_json]
#endif
      ;
*/

acceptable_types::acceptable_types(const std::string &accept_header) {
  using boost::spirit::ascii::blank;
  using iterator_type = std::string::const_iterator;
  using grammar = http_accept_grammar<iterator_type>;

  std::vector<media_range> ranges;
  grammar g;
  iterator_type itr = accept_header.begin();
  iterator_type end = accept_header.end();
  bool status = phrase_parse(itr, end, g, blank, ranges);

  if (status && (itr == end)) {
    for (media_range range : ranges) {
      // figure out the mime::type from the string.
      mime::type mime_type = mime::parse_from(range.mime_type);
      if (mime_type == mime::type::unspecified_type) {
        // if it's unknown then skip this type...
        continue;
      }

      // figure out the quality
      auto q_itr = range.params.find("q");
      // default quality parameter is 1
      float quality = 1.0;
      if (q_itr != range.params.end()) {
        quality = std::stof(q_itr->second);
      }

      mapping.insert(make_pair(mime_type, quality));
    }

  } else {
    logger::message(fmt::format("Failed to parse accept header '{}'",
                    accept_header));
    throw http::bad_request("Accept header could not be parsed.");
  }
}

bool acceptable_types::is_acceptable(mime::type mt) const {
  if (mapping.find(mime::type::any_type) != mapping.end())
    return true;
  return mapping.find(mt) != mapping.end();
}

mime::type acceptable_types::most_acceptable_of(const std::vector<mime::type> &available) const {
  mime::type best = mime::type::unspecified_type;
  float score = std::numeric_limits<float>::min();
  for (mime::type type : available) {
    auto itr = mapping.find(type);
    if ((itr != mapping.end()) && (itr->second > score)) {
      best = itr->first;
      score = itr->second;
    }
  }

  // todo: check the partial wildcards.

  // also check the full wildcard.
  if (!available.empty()) {
    auto itr = mapping.find(mime::type::any_type);
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
acceptable_types header_mime_type(const request &req) {
  // need to look at HTTP_ACCEPT request environment
  string accept_header = fcgi_get_env(req, "HTTP_ACCEPT", "*/*");
  return acceptable_types(accept_header);
}

std::string mime_types_to_string(const std::vector<mime::type> &mime_types)
{
  std::string result;

  for (const auto& m : mime_types) {
    if (!result.empty()) {
	result += ", ";
    }
    result += mime::to_string(m);
  }
  return result;
}

}

mime::type choose_best_mime_type(const request &req, const responder& hptr) {
  // figure out what, if any, the Accept-able resource mime types are
  acceptable_types types = header_mime_type(req);
  const std::vector<mime::type> types_available = hptr.types_available();

  mime::type best_type = hptr.resource_type();
  // check if the handler is capable of supporting an acceptable set of mime
  // types.
  if (best_type != mime::type::unspecified_type) {
    // check that this doesn't conflict with anything in the Accept header.
    if (!hptr.is_available(best_type))
      throw http::not_acceptable(fmt::format("Acceptable formats for {} are: {}",
                                 get_request_path(req),
                                 mime_types_to_string(types_available)));
    else if (!types.is_acceptable(best_type))
      throw http::not_acceptable(fmt::format("Acceptable formats for {} are: {}",
                                 get_request_path(req),
                                 mime_types_to_string({best_type})));
  } else {
    best_type = types.most_acceptable_of(types_available);
    // if none were acceptable then...
    if (best_type == mime::type::unspecified_type) {
	      throw http::not_acceptable(fmt::format("Acceptable formats for {} are: {}",
	                                get_request_path(req),
					mime_types_to_string(types_available)));
    } else if (best_type == mime::type::any_type) {
      // choose the first of the available types if nothing is preferred.
      best_type = *(hptr.types_available().begin());
    }
    // otherwise we've chosen the most acceptable and available type...
  }

  return best_type;
}

std::unique_ptr<output_formatter> create_formatter(mime::type best_type, output_buffer& out) {

  switch (best_type) {
    case mime::type::application_xml:
      return std::make_unique<xml_formatter>(std::make_unique<xml_writer>(out, true));

#if HAVE_YAJL
    case mime::type::application_json:
      return std::make_unique<json_formatter>(std::make_unique<json_writer>(out, false));
#endif
    case mime::type::text_plain:
      return std::make_unique<text_formatter>(std::make_unique<text_writer>(out, true));

    default:
      throw std::runtime_error(fmt::format("Could not create formatter for MIME type `{}'.", mime::to_string(best_type)));
  }
}


