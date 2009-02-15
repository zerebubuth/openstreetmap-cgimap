#include "split_tags.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

using namespace std;
namespace al = boost::algorithm;

/**
 * unescapes a string. i'd love to do this in a *nice* way like in the
 * 0.5 API code, but apparently all the C++ string replacement libraries
 * are ugly and retarded.
 */
string
unescape_string(const string &in) {
  bool escape_mode = false;
  string out;

  out.reserve(in.size());

  for (string::const_iterator itr = in.begin();
       itr != in.end(); ++itr) {
    if (escape_mode) {
      if (*itr == 'e') {
        out.push_back('=');
      } else if (*itr == 's') {
        out.push_back(';');
      } else if (*itr == '\\') {
        out.push_back('\\');
      } else {
        //throw runtime_error("Invalid escape character.");
        // if the escape sequence doesn't match up, just assume that it
        // wasn't meant to be an escape character... or something like that.
        out.push_back('\\');
        out.push_back(*itr);
      }
      escape_mode = false;

    } else {
      if (*itr == '\\') {
        escape_mode = true;
      } else {
        out.push_back(*itr);
      }
    }
  }
  if (escape_mode) {
    throw runtime_error("Invalid escape at end-of-string.");
  }

  return out;
}

/**
 * splits a string into a k=>v map (as even/odd pairs in a vector) for tags as
 * described in Tags.split in the 0.5 API code.
 */
vector<string>
tags_split(string str) {
  vector<string> strs, kvs;

  // first, split the string on semicolon to get key-value
  // pairs as a string array.
  al::split(strs, str, al::is_any_of(";"));
  kvs.reserve(strs.size() * 2);

  // for each of those, we then split on equals
  for (vector<string>::iterator itr = strs.begin();
       itr != strs.end(); ++itr) {
    vector<string> keyval;
    al::split(keyval, *itr, al::is_any_of("="));
    // both key and value must be present, according to the split rules in
    // the API 0.5 ruby code.
    if ((keyval.size() > 1) &&
        (keyval[0].size() > 0) &&
        (keyval[1].size() > 0)) {
      kvs.push_back(unescape_string(keyval[0]));
      kvs.push_back(unescape_string(keyval[1]));
    }
  }

  return kvs;
}
