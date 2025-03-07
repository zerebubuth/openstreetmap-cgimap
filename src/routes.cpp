/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/routes.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/util.hpp"

/*** API 0.6 ***/
#include "cgimap/api06/map_handler.hpp"

#include "cgimap/api06/node_handler.hpp"
#include "cgimap/api06/node_version_handler.hpp"
#include "cgimap/api06/nodes_handler.hpp"
#include "cgimap/api06/node_history_handler.hpp"

#include "cgimap/api06/way_handler.hpp"
#include "cgimap/api06/way_version_handler.hpp"
#include "cgimap/api06/way_full_handler.hpp"
#include "cgimap/api06/ways_handler.hpp"
#include "cgimap/api06/way_history_handler.hpp"

#include "cgimap/api06/relation_handler.hpp"
#include "cgimap/api06/relation_version_handler.hpp"
#include "cgimap/api06/relation_full_handler.hpp"
#include "cgimap/api06/relations_handler.hpp"
#include "cgimap/api06/relation_history_handler.hpp"

#include "cgimap/api06/changeset_handler.hpp"
#include "cgimap/api06/changeset_download_handler.hpp"
#include "cgimap/api06/changeset_upload_handler.hpp"
#include "cgimap/api06/changeset_create_handler.hpp"
#include "cgimap/api06/changeset_update_handler.hpp"
#include "cgimap/api06/changeset_close_handler.hpp"

#include "cgimap/api06/node_ways_handler.hpp"
#include "cgimap/api06/node_relations_handler.hpp"
#include "cgimap/api06/way_relations_handler.hpp"
#include "cgimap/api06/relation_relations_handler.hpp"

#ifdef ENABLE_API07
/*** API 0.7 ***/
#include "cgimap/api07/map_handler.hpp"
#endif /* ENABLE_API07 */

#include "cgimap/router.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/mime_types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include <fmt/core.h>


/**
 * maps router DSL expressions to constructors for handlers. this means it's
 * possible to write .add<Type>(root_/int_/int_) and have the Type handler
 * constructed with the request and two ints as parameters.
 */
struct router {
  // interface through which all matches and constructions are performed.
  struct rule_base {
    virtual ~rule_base() = default;
    virtual bool invoke_if(const std::vector<std::string_view> &, request &, handler_ptr_t &) = 0;
  };

  // concrete rule match / constructor class
  template <typename Handler, typename Rule>
  struct rule : public rule_base {
    explicit constexpr rule(Rule&& r) : r(std::forward<Rule>(r)) {}

    // try to match the expression. if it succeeds, call the provided function
    // with the provided params and the matched DSL arguments.
    bool invoke_if(const std::vector<std::string_view> &parts,
                   request &params,
                   handler_ptr_t &ptr) override {

      auto begin = parts.begin();
      auto [sequence, error] = r.match(begin, parts.end());

      if (error)
        return false;

      if (begin != parts.end())
        return false; // no match

      ptr.reset(std::apply(
          [&params](auto &&...args) {
            return new Handler(params, std::forward<decltype(args)>(args)...);
          },
          sequence));

      return true;
    }

    private:
      // the DSL rule expression to match
      Rule r;
  };

  /* add a match all methods rule (a DSL expression) which constructs the Handler
   * type when the rule matches. the Rule type can be inferred,
   * so generally this can be written as
   *  r.all<Handler>(...);
   */

  // add rule to match HTTP GET method only
  template <typename Handler, typename Rule> 
  router& GET(Rule&& r) {

    static_assert(std::is_base_of_v<handler, Handler>, "GET rule requires handler subclass");
    static_assert(!std::is_base_of_v<payload_enabled_handler, Handler>, "GET rule cannot use payload enabled handler subclass");

    rules_get.push_back(std::make_unique<rule<Handler, Rule> >(std::forward<Rule>(r)));
    return *this;
  }

  // add rule to match HTTP POST method only
  template <typename Handler, typename Rule> 
  router& POST(Rule&& r) {

    static_assert(std::is_base_of_v<payload_enabled_handler, Handler>, "POST rule requires payload enabled handler subclass");

    rules_post.push_back(std::make_unique<rule<Handler, Rule> >(std::forward<Rule>(r)));
    return *this;
  }

  // add rule to match HTTP PUT method only
  template <typename Handler, typename Rule> 
  router& PUT(Rule&& r) {

    static_assert(std::is_base_of_v<payload_enabled_handler, Handler>, "PUT rule requires payload enabled handler subclass");

    rules_put.push_back(std::make_unique<rule<Handler, Rule> >(std::forward<Rule>(r)));
    return *this;
  }

  /* match the list of path components given in p. if a match is found,
   * construct an object of the handler type with the provided params
   * and the matched params.
   */

  handler_ptr_t match(const std::vector<std::string_view> &p, request &params) const {

    handler_ptr_t hptr;

    http::method allowed_methods = http::method::OPTIONS;

    // it probably isn't necessary to have any more sophisticated data structure
    // than a list at this point. also means the semantics for rule matching are
    // pretty clear - the first match wins.

    auto maybe_method = http::parse_method(fcgi_get_env(params, "REQUEST_METHOD"));

    if (!maybe_method)
      return hptr;

    // Process HEAD like GET, as per rfc2616: The HEAD method is identical to
    // GET except that the server MUST NOT return a message-body in the response.
    for (const auto& rptr : rules_get) {
      if (rptr->invoke_if(p, params, hptr)) {
          if (*maybe_method == http::method::GET    ||
        *maybe_method == http::method::HEAD   ||
        *maybe_method == http::method::OPTIONS)
            return hptr;
          allowed_methods |= http::method::GET | http::method::HEAD;
      }
    }

    for (const auto& rptr : rules_post) {
      if (rptr->invoke_if(p, params, hptr)) {
          if (*maybe_method == http::method::POST||
        *maybe_method == http::method::OPTIONS)
            return hptr;
          allowed_methods |= http::method::POST;
      }
    }

    for (const auto& rptr : rules_put) {
      if (rptr->invoke_if(p, params, hptr)) {
          if (*maybe_method == http::method::PUT||
        *maybe_method == http::method::OPTIONS)
            return hptr;
          allowed_methods |= http::method::PUT;
      }
    }

    // Did the request URL path match one of the rules, yet the request method didn't match?
    // This assumes that some additional request methods have been added to allowed_methods
    // on top of the initial OPTIONS value.
    if (allowed_methods != http::method::OPTIONS) {
	     // return a 405 HTTP Method not allowed error
       throw http::method_not_allowed(allowed_methods);
    }

    // return pointer to nothing if no matches found.
    return hptr;
  }

private:
  using rule_ptr = std::unique_ptr<rule_base>;

  std::vector<rule_ptr> rules_get;
  std::vector<rule_ptr> rules_post;
  std::vector<rule_ptr> rules_put;
};

routes::routes()
    : common_prefix("/api/0.6/"),
      r(std::make_unique<router>())
#ifdef ENABLE_API07
      ,
      experimental_prefix("/api/0.7/"),
      r_experimental(new router())
#endif /* ENABLE_API07 */
{
  using match::root_;
  using match::osm_id_;

  {
    using namespace api06;
    r->GET<map_handler>(root_ / "map")
      .GET<node_ways_handler>(root_ / "node" / osm_id_ / "ways")
      .GET<node_relations_handler>(root_ / "node" / osm_id_ / "relations")

    // make sure that *_version_handler is listed before matching *_handler
      .GET<node_history_handler>(root_ / "node" / osm_id_ / "history")
      .GET<node_version_handler>(root_ / "node" / osm_id_ / osm_id_ )
      .GET<node_handler>(root_ / "node" / osm_id_)
      .GET<nodes_handler>(root_ / "nodes")

      .GET<way_full_handler>(root_ / "way" / osm_id_ / "full")
      .GET<way_relations_handler>(root_ / "way" / osm_id_ / "relations")
      .GET<way_history_handler>(root_ / "way" / osm_id_ / "history")
      .GET<way_version_handler>(root_ / "way" / osm_id_ / osm_id_ )
      .GET<way_handler>(root_ / "way" / osm_id_)
      .GET<ways_handler>(root_ / "ways")

      .GET<relation_full_handler>(root_ / "relation" / osm_id_ / "full")
      .GET<relation_relations_handler>(root_ / "relation" / osm_id_ / "relations")
      .GET<relation_history_handler>(root_ / "relation" / osm_id_ / "history")
      .GET<relation_version_handler>(root_ / "relation" / osm_id_ / osm_id_ )
      .GET<relation_handler>(root_ / "relation" / osm_id_)
      .GET<relations_handler>(root_ / "relations")

      .GET<changeset_download_handler>(root_ / "changeset" / osm_id_ / "download")
      .POST<changeset_upload_handler>(root_ / "changeset" / osm_id_ / "upload")
      .PUT<changeset_close_handler>(root_ / "changeset" / osm_id_ / "close")
      .PUT<changeset_create_handler>(root_ / "changeset" / "create")
      .PUT<changeset_update_handler>(root_ / "changeset" / osm_id_)
      .GET<changeset_handler>(root_ / "changeset" / osm_id_);
  }

#ifdef ENABLE_API07
  {
    using namespace api07;
    r_experimental->GET<map_handler>(root_ / "map")
                   .GET<map_handler>(root_ / "map" / "tile" / osm_id_);
  }
#endif /* ENABLE_API07 */
}

routes::~routes() = default;

namespace {
/**
 * figures out the mime type from the path specification, e.g: a resource ending
 * in .xml should be application/xml, .json should be application/json, etc...
 */
  std::pair<std::string, mime::type> resource_mime_type(const std::string &path) {

  using enum mime::type;

  if (path.ends_with(".json")) {
      return {path.substr(0, path.length() - 5), application_json};
  }

  if (path.ends_with(".xml")) {
      return {path.substr(0, path.length() - 4), application_xml};
  }

  return {path, unspecified_type};
}

handler_ptr_t route_resource(request &req, const std::string &path,
                             const router * r) {

  // strip off the format-spec, if there is one
  auto [resource, mime_type] = resource_mime_type(path);

  // split the URL into bits to be matched.
  auto path_components = split<std::string_view>(resource, '/');

  auto hptr(r->match(path_components, req));

  // if the pointer points at something, then the path was found. otherwise,
  // it must have exhausted all the possible routes.
  if (hptr) {
    // ugly hack - need this info later on to choose the output formatter,
    // but don't want to parse the URI again...
    hptr->set_resource_type(mime_type);
  }

  return hptr;
}
} // anonymous namespace

handler_ptr_t routes::operator()(request &req) const {
  // full path from request handler
  auto path = get_request_path(req);
  handler_ptr_t hptr;
  // check the prefix
  if (path.starts_with(common_prefix)) {
    hptr = route_resource(req, std::string(path, common_prefix.size()), r.get());

#ifdef ENABLE_API07
  } else if (path.starts_with(experimental_prefix)) {
    hptr = route_resource(req, std::string(path, experimental_prefix.size()),
                          r_experimental.get());
#endif /* ENABLE_API07 */
  }

  if (!hptr) {
    // doesn't match prefix...
    throw http::not_found(fmt::format("Path does not match any known routes: {}", path));
  }

  return hptr;
}
