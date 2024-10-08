/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "cgimap/types.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/fusion/container/generation/make_cons.hpp>
#include <boost/fusion/include/make_cons.hpp>
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/include/list.hpp>
#include <boost/fusion/algorithm/transformation/join.hpp>
#include <boost/fusion/include/join.hpp>
#include <boost/fusion/sequence/io.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/fusion/container/list/convert.hpp>
#include <boost/fusion/include/as_list.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/function_types/function_type.hpp>
#include <boost/fusion/functional/invocation/invoke.hpp>
#include <boost/functional/factory.hpp>

namespace match {

using boost::fusion::list;
using boost::fusion::join;
using boost::fusion::make_cons;
using boost::fusion::as_list;
namespace result_of = boost::fusion::result_of;

// iterates over the split up parts of the item being matched.
using part_iterator = std::vector<std::string_view>::const_iterator;

/**
 * thrown when a match error occurs, giving some information about the
 * reason for the error.
 */
struct error : public std::runtime_error {
  error();
};

/* predeclarations of the AST classes which can be matched. since an
 * expression is used to build up the match, many of these have to refer
 * to each other.
 *
 * the AST classes all have a type match_type which indicates the tuple
 * they return if they do not throw a match error. also, each have a
 * match() method which checks whether the AST parses correctly and
 * returns the full type indicated by match_type.
 */
struct match_string;
struct match_osm_id;
struct match_begin;

template <typename LeftType, typename RightType> struct match_and;

/**
 * utility class included to map the necessary operators to form the match
 * expression to the construction of the AST objects. the reason these are
 * explicitly declared, rather than templated, is to allow the compiler to
 * correctly generate implicit constructors. this makes it much more
 * readable (e.g: "foo"/"bar" rather than match_string("foo") /
 * match_string("bar")).
 */
template <typename Self> struct ops {
  match_and<Self, match_string> operator/(const match_string &rhs) const {
    return match_and<Self, match_string>(*static_cast<const Self *>(this), rhs);
  }
  match_and<Self, match_osm_id> operator/(const match_osm_id &rhs) const {
    return match_and<Self, match_osm_id>(*static_cast<const Self *>(this), rhs);
  }
};

/**
 * effectively a cons cell for sequencing matches.
 */
template <typename LeftType, typename RightType>
struct match_and : public ops<match_and<LeftType, RightType> > {

  using match_type = typename result_of::as_list<typename result_of::join<
      typename LeftType::match_type,
      typename RightType::match_type>::type>::type;

  match_and(const LeftType &l, const RightType &r) : lhs(l), rhs(r) {}

  std::pair<match_type, bool> match(part_iterator &begin, const part_iterator &end) const {
    auto [ lval, lerror ] = lhs.match(begin, end);
    if (lerror)
      return {as_list(join(typename LeftType::match_type(), typename RightType::match_type())), true};
    auto [ rval, rerror ] = rhs.match(begin, end);
    if (rerror)
      return {as_list(join(typename LeftType::match_type(), typename RightType::match_type())), true};
    return {as_list(join(lval, rval)), false};
  }

private:
  LeftType lhs;
  RightType rhs;
};

/**
 * matches a literal string, passed in the constructor.
 */
struct match_string : public ops<match_string> {
  // doesn't return anything, simply fails if the string doesn't match.
  using match_type = list<>;

  // implicit constructor intended, so that the use of this class is
  // hidden and easier / nicer to read.
  match_string(const char *s);

  // copy just copies the held string
  inline match_string(const match_string &m) = default;

  std::pair<match_type, bool> match(part_iterator &begin, const part_iterator &end) const noexcept;

private:
  std::string_view str;
};

/**
 * match an OSM ID, returning it in the match tuple.
 */
struct match_osm_id : public ops<match_osm_id> {
  using match_type = list<osm_nwr_id_t>;
  match_osm_id() = default;
  std::pair<match_type, bool> match(part_iterator &begin, const part_iterator &end) const noexcept;
};

/**
 * null match - it'll match anything. it's only here to anchor the expression
 * with the correct type, allowing us to write the rest of the expression
 * without needing explicit constructors for the string literal matches.
 */
struct match_begin : public ops<match_begin> {
  using match_type = list<>;
  match_begin() = default;
  inline std::pair<match_type, bool> match(part_iterator &begin, const part_iterator &end) const noexcept{
    return {match_type(), false};
  }
};

// match items, given nicer names so that expressions are easier to read.
static constexpr match_begin root_;
static constexpr match_osm_id osm_id_;
}

#endif /* ROUTER_HPP */
