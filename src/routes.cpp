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
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include <fmt/core.h>

// The following two concepts validate that the handler has been derived from
// the appropriate based class. NoPayload is suitable for GET requests, while
// POST and PUT requests require a payload enabled handler, which also passes
// the HTTP body to the handler class.

template <typename Handler>
concept NoPayload = std::is_base_of_v<handler, Handler> &&    // rule requires handler subclass
                    !std::is_base_of_v<payload_enabled_handler, Handler>;  // rule cannot use payload enabled handler subclass

template <typename Handler>
concept Payload = std::is_base_of_v<payload_enabled_handler, Handler>; // Payload enabled handler subclass required

/**
 * maps router DSL expressions to constructors for handlers. this means it's
 * possible to write .add<Type>(root/int/int) and have the Type handler
 * constructed with the request and two ints as parameters.
 */
struct router {
  // interface through which all matches and constructions are performed.
  struct rule_base {
    virtual ~rule_base() = default;
    virtual std::unique_ptr<handler> invoke_if(const std::vector<std::string_view> &, request &) = 0;
  };

  // concrete rule match / constructor class
  template <typename Handler, typename Rule>
  struct rule : public rule_base {
    explicit constexpr rule(Rule&& r) : r(std::forward<Rule>(r)) {}

    // try to match the expression. if it succeeds, call the Handler constructor
    // with the request ("params") and the matched DSL arguments ("sequence", a tuple).
    std::unique_ptr<handler> invoke_if(const std::vector<std::string_view> &parts,
                                       request &params) override {

      auto begin = parts.begin();
      auto [sequence, error] = r.match(begin, parts.end());

      if (error)  // no match
        return nullptr;

      if (begin != parts.end()) // still some unmatched parts left
        return nullptr;

      // construct new Handler instance with matched parameters
      return std::apply(
          [&params](auto &&...args) {
            return std::make_unique<Handler>(
                params, std::forward<decltype(args)>(args)...);
          },
          sequence);
    }

    private:
      // the DSL rule expression to match
      Rule r;
  };

  // add rule to match HTTP GET method only
  template <NoPayload Handler, typename Rule>
  router& GET(Rule&& r) {

    rules_get.push_back(std::make_unique<rule<Handler, Rule> >(std::forward<Rule>(r)));
    return *this;
  }

  // add rule to match HTTP POST method only
  template <Payload Handler, typename Rule>
  router& POST(Rule&& r) {

    rules_post.push_back(std::make_unique<rule<Handler, Rule> >(std::forward<Rule>(r)));
    return *this;
  }

  // add rule to match HTTP PUT method only
  template <Payload Handler, typename Rule>
  router& PUT(Rule&& r) {

    rules_put.push_back(std::make_unique<rule<Handler, Rule> >(std::forward<Rule>(r)));
    return *this;
  }

  /* match the list of path components given in p. if a match is found,
   * construct an object of the handler type with the provided params
   * and the matched params.
   */

  std::unique_ptr<handler> match(const std::vector<std::string_view> &p, request &params) const {

    http::method allowed_methods = http::method::OPTIONS;

    // it probably isn't necessary to have any more sophisticated data structure
    // than a list at this point. also means the semantics for rule matching are
    // pretty clear - the first match wins.

    auto maybe_method = http::parse_method(fcgi_get_env(params, "REQUEST_METHOD"));

    if (!maybe_method)
      return nullptr;

    // Process HEAD like GET, as per rfc2616: The HEAD method is identical to
    // GET except that the server MUST NOT return a message-body in the response.
    for (const auto &rptr : rules_get) {
      if (auto hptr = rptr->invoke_if(p, params); hptr) {
        if (*maybe_method == http::method::GET ||
            *maybe_method == http::method::HEAD ||
            *maybe_method == http::method::OPTIONS)
          return hptr;
        allowed_methods |= http::method::GET | http::method::HEAD;
      }
    }

    for (const auto &rptr : rules_post) {
      if (auto hptr = rptr->invoke_if(p, params); hptr) {
        if (*maybe_method == http::method::POST ||
            *maybe_method == http::method::OPTIONS)
          return hptr;
        allowed_methods |= http::method::POST;
      }
    }

    for (const auto &rptr : rules_put) {
      if (auto hptr = rptr->invoke_if(p, params); hptr) {
        if (*maybe_method == http::method::PUT ||
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
    return nullptr;
  }

private:
  using rule_ptr = std::unique_ptr<rule_base>;

  std::vector<rule_ptr> rules_get;
  std::vector<rule_ptr> rules_post;
  std::vector<rule_ptr> rules_put;
};

routes::routes()
    : common_prefix("/api/0.6/"),
      r(get_default_router())
#ifdef ENABLE_API07
      ,
      experimental_prefix("/api/0.7/"),
      r_experimental(get_experimental_router())
#endif /* ENABLE_API07 */
{
}

std::unique_ptr<router> routes::get_default_router()
{
  auto r = std::make_unique<router>();
  using match::root;
  using match::osm_id;

  using namespace api06;
  r->GET<map_handler>(root / "map")
    .GET<node_ways_handler>(root / "node" / osm_id / "ways")
    .GET<node_relations_handler>(root / "node" / osm_id / "relations")

  // make sure that *_version_handler is listed before matching handler
    .GET<node_history_handler>(root / "node" / osm_id / "history")
    .GET<node_version_handler>(root / "node" / osm_id / osm_id)
    .GET<node_handler>(root / "node" / osm_id)
    .GET<nodes_handler>(root / "nodes")

    .GET<way_full_handler>(root / "way" / osm_id / "full")
    .GET<way_relations_handler>(root / "way" / osm_id / "relations")
    .GET<way_history_handler>(root / "way" / osm_id / "history")
    .GET<way_version_handler>(root / "way" / osm_id / osm_id)
    .GET<way_handler>(root / "way" / osm_id)
    .GET<ways_handler>(root / "ways")

    .GET<relation_full_handler>(root / "relation" / osm_id / "full")
    .GET<relation_relations_handler>(root / "relation" / osm_id / "relations")
    .GET<relation_history_handler>(root / "relation" / osm_id / "history")
    .GET<relation_version_handler>(root / "relation" / osm_id / osm_id)
    .GET<relation_handler>(root / "relation" / osm_id)
    .GET<relations_handler>(root / "relations")

    .GET<changeset_download_handler>(root / "changeset" / osm_id / "download")
    .POST<changeset_upload_handler>(root / "changeset" / osm_id / "upload")
    .PUT<changeset_close_handler>(root / "changeset" / osm_id / "close")
    .PUT<changeset_create_handler>(root / "changeset" / "create")
    .PUT<changeset_update_handler>(root / "changeset" / osm_id)
    .GET<changeset_handler>(root / "changeset" / osm_id);

  return r;
}

#ifdef ENABLE_API07
std::unique_ptr<router> routes::get_experimental_router()
{
  auto r = std::make_unique<router>();
  using match::root;
  using match::osm_id;

  using namespace api07;
  r->GET<map_handler>(root / "map")
    .GET<map_handler>(root / "map" / "tile" / osm_id);

  return r;
}
#endif /* ENABLE_API07 */

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

std::unique_ptr<handler> route_resource(request &req, const std::string &path,
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

std::unique_ptr<handler> routes::operator()(request &req) const {
  // full path from request handler
  auto path = get_request_path(req);
  std::unique_ptr<handler> hptr;
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
