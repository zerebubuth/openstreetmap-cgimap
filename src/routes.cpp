#include "cgimap/config.hpp"
#include "cgimap/routes.hpp"
#include "cgimap/handler.hpp"

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

#include <optional>

#include <boost/algorithm/string.hpp>

using std::list;
using std::string;
using std::pair;
using std::unique_ptr;

using boost::fusion::make_cons;
using boost::fusion::invoke;

namespace al = boost::algorithm;

/**
 * maps router DSL expressions to constructors for handlers. this means it's
 * possible to write .add<Type>(root_/int_/int_) and have the Type handler
 * constructed with the request and two ints as parameters.
 */
struct router {
  // interface through which all matches and constructions are performed.
  struct rule_base {
    virtual ~rule_base() = default;
    virtual bool invoke_if(const list<string> &, request &,
                           handler_ptr_t &) = 0;
  };

  using rule_ptr = std::unique_ptr<rule_base>;

  // concrete rule match / constructor class
  template <typename rule_t, typename func_t> struct rule : public rule_base {
    // the DSL rule expression to match
    rule_t r;

    // the function to call (used later as constructor factory)
    func_t func;

    rule(rule_t r_, func_t f_) : r(r_), func(f_) {}

    // try to match the expression. if it succeeds, call the provided function
    // with the provided params and the matched DSL arguments.
    bool invoke_if(const list<string> &parts, request &params,
                   handler_ptr_t &ptr) {
      try {
        auto begin = parts.begin();
        auto sequence = r.match(begin, parts.end());
        if(begin!=parts.end())
          throw match::error();
        ptr.reset(
            invoke(func, make_cons(std::ref(params), sequence)));
        return true;
      } catch (const match::error &e) {
        return false;
      }
    }
  };

  /* add a match all methods rule (a DSL expression) which constructs the Handler
   * type when the rule matches. the Rule type can be inferred,
   * so generally this can be written as
   *  r.all<Handler>(...);
   */

  // add rule to match HTTP GET method only
  template <typename Handler, typename Rule> router& GET(Rule r) {
    // functor to create Handler instances
    boost::factory<Handler *> ctor;

    static_assert(std::is_base_of<handler, Handler>::value, "GET rule requires handler subclass");
    static_assert(!std::is_base_of<payload_enabled_handler, Handler>::value, "GET rule cannot use payload enabled handler subclass");

    rules_get.push_back(
        rule_ptr(new rule<Rule, boost::factory<Handler *> >(r, ctor)));
    return *this;
  }

  // add rule to match HTTP POST method only
  template <typename Handler, typename Rule> router& POST(Rule r) {
    // functor to create Handler instances
    boost::factory<Handler *> ctor;

    static_assert(std::is_base_of<payload_enabled_handler, Handler>::value, "POST rule requires payload enabled handler subclass");

    rules_post.push_back(
        rule_ptr(new rule<Rule, boost::factory<Handler *> >(r, ctor)));
    return *this;
  }


  // add rule to match HTTP PUT method only
  template <typename Handler, typename Rule> router& PUT(Rule r) {
    // functor to create Handler instances
    boost::factory<Handler *> ctor;

    static_assert(std::is_base_of<payload_enabled_handler, Handler>::value, "PUT rule requires payload enabled handler subclass");

    rules_put.push_back(
        rule_ptr(new rule<Rule, boost::factory<Handler *> >(r, ctor)));
    return *this;
  }

  /* match the list of path components given in p. if a match is found,
   * construct an
   * object of the handler type with the provided params and the matched
   * params.
   */

  handler_ptr_t match(const list<string> &p, request &params) {

    handler_ptr_t hptr;

    http::method allowed_methods = http::method::OPTIONS;

    // it probably isn't necessary to have any more sophisticated data structure
    // than a list at this point. also means the semantics for rule matching are
    // pretty clear - the first match wins.

    std::optional<http::method> maybe_method =
	http::parse_method(fcgi_get_env(params, "REQUEST_METHOD"));

    if (!maybe_method)
      return hptr;

    // Process HEAD like GET, as per rfc2616: The HEAD method is identical to
    // GET except that the server MUST NOT return a message-body in the response.
    for (auto& rptr : rules_get) {
	if (rptr->invoke_if(p, params, hptr)) {
	    if (*maybe_method == http::method::GET    ||
		*maybe_method == http::method::HEAD   ||
		*maybe_method == http::method::OPTIONS)
	      return hptr;
	    allowed_methods |= http::method::GET | http::method::HEAD;
	}
    }

    for (auto& rptr : rules_post) {
	if (rptr->invoke_if(p, params, hptr)) {
	    if (*maybe_method == http::method::POST||
		*maybe_method == http::method::OPTIONS)
	      return hptr;
	    allowed_methods |= http::method::POST;
	}
    }

    for (auto& rptr : rules_put) {
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
  list<rule_ptr> rules_get;
  list<rule_ptr> rules_post;
  list<rule_ptr> rules_put;
};

routes::routes()
    : common_prefix("/api/0.6/"),
      r(new router())
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
  pair<string, mime::type> resource_mime_type(const string &path) {

#ifdef HAVE_YAJL
    {
      std::size_t json_found = path.rfind(".json");

      if (json_found != string::npos && json_found == path.length() - 5) {
	  return make_pair(path.substr(0, json_found), mime::application_json);
      }
    }
#endif

    {
      std::size_t xml_found = path.rfind(".xml");

      if (xml_found != string::npos && xml_found == path.length() - 4) {
	  return make_pair(path.substr(0, xml_found), mime::application_xml);
      }
    }

    return make_pair(path, mime::unspecified_type);
}

handler_ptr_t route_resource(request &req, const string &path,
                             const unique_ptr<router> &r) {
  // strip off the format-spec, if there is one
  pair<string, mime::type> resource = resource_mime_type(path);

  // split the URL into bits to be matched.
  list<string> path_components;
  al::split(path_components, resource.first, al::is_any_of("/"));

  handler_ptr_t hptr(r->match(path_components, req));

  // if the pointer points at something, then the path was found. otherwise,
  // it must have exhausted all the possible routes.
  if (hptr) {
    // ugly hack - need this info later on to choose the output formatter,
    // but don't want to parse the URI again...
    hptr->set_resource_type(resource.second);
  }

  return hptr;
}
} // anonymous namespace

handler_ptr_t routes::operator()(request &req) const {
  // full path from request handler
  string path = get_request_path(req);
  handler_ptr_t hptr;
  // check the prefix
  if (path.compare(0, common_prefix.size(), common_prefix) == 0) {
    hptr = route_resource(req, string(path, common_prefix.size()), r);

#ifdef ENABLE_API07
  } else if (path.compare(0, experimental_prefix.size(), experimental_prefix) ==
             0) {
    hptr = route_resource(req, string(path, experimental_prefix.size()),
                          r_experimental);
#endif /* ENABLE_API07 */
  }

  if (hptr) {
    return hptr;

  } else {
    // doesn't match prefix...
    throw http::not_found(fmt::format("Path does not match any known routes: {}", path));
  }
}
