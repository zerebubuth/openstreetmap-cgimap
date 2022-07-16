#include "cgimap/api06/handler_utils.hpp"

#include "cgimap/http.hpp"
#include "cgimap/request_helpers.hpp"
#include <algorithm>
#include <map>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <fmt/core.h>

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
namespace standard = boost::spirit::standard;

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
  (std::optional<uint32_t>, version)
  )

namespace {

template <typename Iterator>
struct id_version_parser : qi::grammar<Iterator, api06::id_version(), standard::space_type> {
  id_version_parser() : id_version_parser::base_type(root) {
    using qi::lit;
    using qi::uint_;
    using boost::spirit::ulong_long;

    root = ulong_long >> -(lit("v") >> uint_);
  }

  qi::rule<Iterator, api06::id_version(), standard::space_type> root;
};

template <typename Iterator>
struct id_version_list_parser
  : qi::grammar<Iterator, std::vector<api06::id_version>(), standard::space_type> {
  id_version_list_parser() : id_version_list_parser::base_type(root) {
    using qi::lit;

    root = idv % lit(",");
  }

  qi::rule<Iterator, std::vector<api06::id_version>(), standard::space_type> root;
  id_version_parser<Iterator> idv;
};

} // anonymous namespace

namespace api06 {

bool valid_string(const std::string& str)
{
  // check if character is representable as an unsigned char
  // see https://www.boost.org/doc/libs/1_77_0/boost/spirit/home/support/char_encoding/standard.hpp

  return std::all_of(str.begin(), str.end(),
		     [](char c){ return c >= 0 && c <= UCHAR_MAX; });
}

vector<id_version> parse_id_list_params(request &req, const string &param_name) {

  string decoded = http::urldecode(get_query_string(req));
  const vector<pair<string, string> > params = http::parse_params(decoded);
  auto itr = std::find_if(params.begin(), params.end(), first_equals(param_name));

  if (itr == params.end())
    return {};

  const string &str = itr->second;

  if (str.empty())
    return {};

  // Make sure our string does not violate boost spirit standard encoding check in strict_ischar
  // Failure to do so triggers assertion failures, unless NDEBUG flag is set during compilation.
  if (!valid_string(str))
    return {};

  vector<id_version> parse_ids;
  vector<id_version> myids;
  string::const_iterator first = str.begin(), last = str.end();
  id_version_list_parser<string::const_iterator> idv_p;

  try {

    bool ok = qi::phrase_parse(
      first, last, idv_p, boost::spirit::qi::standard::space, parse_ids);

    if (ok && (first == last)) {
      myids.swap(parse_ids);
    } else {
       myids.push_back(id_version());
    }
  } catch (...) {   // input could not be parsed, ignore
      return {};
  }

  // ensure list of IDs is unique
  std::sort(myids.begin(), myids.end());
  auto new_end = std::unique(myids.begin(), myids.end());
  myids.erase(new_end, myids.end());

  return myids;
}
}
