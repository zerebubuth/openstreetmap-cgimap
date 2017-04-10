#include "cgimap/api06/handler_utils.hpp"

#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include <map>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

using std::list;
using std::string;
using std::map;
using std::vector;
using std::pair;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

namespace {
struct first_equals {
  first_equals(const std::string &k) : m_key(k) {}
  bool operator()(const pair<string, string> &v) const {
    return v.first == m_key;
  }
  string m_key;
};
} // anonymous namespace

BOOST_FUSION_ADAPT_STRUCT(
  api06::id_version,
  (uint64_t, id)
  (boost::optional<uint32_t>, version)
  )

namespace {

template <typename Iterator>
struct id_version_parser : qi::grammar<Iterator, api06::id_version(), ascii::space_type> {
  id_version_parser() : id_version_parser::base_type(root) {
    using qi::lit;
    using qi::uint_;
    using boost::spirit::ulong_long;

    root = ulong_long >> -(lit("v") >> uint_);
  }

  qi::rule<Iterator, api06::id_version(), ascii::space_type> root;
};

template <typename Iterator>
struct id_version_list_parser
  : qi::grammar<Iterator, std::vector<api06::id_version>(), ascii::space_type> {
  id_version_list_parser() : id_version_list_parser::base_type(root) {
    using qi::lit;

    root = idv % lit(",");
  }

  qi::rule<Iterator, std::vector<api06::id_version>(), ascii::space_type> root;
  id_version_parser<Iterator> idv;
};

} // anonymous namespace

namespace api06 {

vector<id_version> parse_id_list_params(request &req, const string &param_name) {
  string decoded = http::urldecode(get_query_string(req));
  const vector<pair<string, string> > params = http::parse_params(decoded);
  vector<pair<string, string> >::const_iterator itr =
    std::find_if(params.begin(), params.end(), first_equals(param_name));

  vector<id_version> myids;

  if (itr != params.end()) {
    vector<id_version> parse_ids;
    const string &str = itr->second;

    if (!str.empty()) {
      string::const_iterator first = str.begin(), last = str.end();
      id_version_list_parser<string::const_iterator> idv_p;

      bool ok = qi::phrase_parse(
        first, last, idv_p, boost::spirit::qi::ascii::space, parse_ids);

      if (ok && (first == last)) {
        myids.swap(parse_ids);
      } else {
        myids.push_back(id_version());
      }
    }
  }

  // ensure list of IDs is unique
  std::sort(myids.begin(), myids.end());
  vector<id_version>::iterator new_end = std::unique(myids.begin(), myids.end());
  myids.erase(new_end, myids.end());

  return myids;
}
}
