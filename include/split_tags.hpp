#ifndef SPLIT_TAGS_HPP
#define SPLIT_TAGS_HPP

#include <string>
#include <vector>

/**
 * unescapes a string. i'd love to do this in a *nice* way like in the
 * 0.5 API code, but apparently all the C++ string replacement libraries
 * are ugly and retarded.
 */
std::string
unescape_string(const std::string &in);

/**
 * splits a string into a k=>v map (as even/odd pairs in a vector) for tags as
 * described in Tags.split in the 0.5 API code.
 */
std::vector<std::string>
tags_split(std::string str);

#endif /* SPLIT_TAGS_HPP */
