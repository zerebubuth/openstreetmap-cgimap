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

#ifdef ENABLE_EXPERIMENTAL
#include "cgimap/api06/node_ways_handler.hpp"
#endif /* ENABLE_EXPERIMENTAL */

#ifdef ENABLE_API07
/*** API 0.7 ***/
#include "cgimap/api07/map_handler.hpp"
#endif /* ENABLE_API07 */

#include "cgimap/router.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/http.hpp"
#include "cgimap/mime_types.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>

using std::auto_ptr;
using std::list;
using std::string;
using std::pair;
using boost::shared_ptr;
using boost::scoped_ptr;
using boost::optional;
using boost::fusion::make_cons;
using boost::fusion::invoke;
using boost::ref;
namespace al = boost::algorithm;

/**
 * maps router DSL expressions to constructors for handlers. this means it's
 * possible to write .add<Type>(root_/int_/int_) and have the Type handler
 * constructed with the request and two ints as parameters.
 */
struct router {
  // interface through which all matches and constructions are performed.
  struct rule_base {
    virtual ~rule_base() {}
    virtual bool invoke_if(const list<string> &, request &,
                           handler_ptr_t &) = 0;
  };

  typedef boost::shared_ptr<rule_base> rule_ptr;

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
        list<string>::const_iterator begin = parts.begin();
        auto sequence = r.match(begin, parts.end());
        if(begin!=parts.end())
          throw match::error();
        ptr.reset(
            invoke(func, make_cons(ref(params), sequence)));
        return true;
      } catch (const match::error &e) {
        return false;
      }
    }
  };

  /* add a rule (a DSL expression) which constructs the Handler type when the
   * rule
         * matches. the Rule type can be inferred, so generally this can be
   * written as
         * r.add<Handler>(...);
         */
  template <typename Handler, typename Rule> void add(Rule r) {
    // functor to create Handler instances
    boost::factory<Handler *> ctor;
    rules.push_back(
        rule_ptr(new rule<Rule, boost::factory<Handler *> >(r, ctor)));
  }

  /* match the list of path components given in p. if a match is found,
   * construct an
         * object of the handler type with the provided params and the matched
   * params.
         */
  handler_ptr_t match(const list<string> &p, request &params) {
    handler_ptr_t hptr;
    // it probably isn't necessary to have any more sophisticated data structure
    // than a list at this point. also means the semantics for rule matching are
    // pretty clear - the first match wins.
    for (list<rule_ptr>::iterator itr = rules.begin(); itr != rules.end();
         ++itr) {
      rule_ptr rptr = *itr;
      if (rptr->invoke_if(p, params, hptr)) {
        break;
      }
    }
    // return pointer to nothing if no matches found.
    return hptr;
  }

private:
  list<rule_ptr> rules;
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
    r->add<map_handler>(root_ / "map");

#ifdef ENABLE_EXPERIMENTAL
    r->add<node_ways_handler>(root_ / "node" / osm_id_ / "ways");
#endif /* ENABLE_EXPERIMENTAL */
    // make sure that *_version_handler is listed before matching *_handler
    r->add<node_history_handler>(root_ / "node" / osm_id_ / "history");
    r->add<node_version_handler>(root_ / "node" / osm_id_ / osm_id_ );
    r->add<node_handler>(root_ / "node" / osm_id_);
    r->add<nodes_handler>(root_ / "nodes");

    r->add<way_full_handler>(root_ / "way" / osm_id_ / "full");
    r->add<way_history_handler>(root_ / "way" / osm_id_ / "history");
    r->add<way_version_handler>(root_ / "way" / osm_id_ / osm_id_ );
    r->add<way_handler>(root_ / "way" / osm_id_);
    r->add<ways_handler>(root_ / "ways");

    r->add<relation_full_handler>(root_ / "relation" / osm_id_ / "full");
    r->add<relation_history_handler>(root_ / "relation" / osm_id_ / "history");
    r->add<relation_version_handler>(root_ / "relation" / osm_id_ / osm_id_ );
    r->add<relation_handler>(root_ / "relation" / osm_id_);
    r->add<relations_handler>(root_ / "relations");

    r->add<changeset_download_handler>(root_ / "changeset" / osm_id_ / "download");
    r->add<changeset_handler>(root_ / "changeset" / osm_id_);
  }

#ifdef ENABLE_API07
  {
    using namespace api07;
    r_experimental->add<map_handler>(root_ / "map");
    r_experimental->add<map_handler>(root_ / "map" / "tile" / osm_id_);
  }
#endif /* ENABLE_API07 */
}

routes::~routes() {}

namespace {
/**
 * figures out the mime type from the path specification, e.g: a resource ending
 * in .xml should be text/xml, .json should be text/json, etc...
 */
pair<string, mime::type> resource_mime_type(const string &path) {
  return make_pair(path, mime::unspecified_type);
}

handler_ptr_t route_resource(request &req, const string &path,
                             const scoped_ptr<router> &r) {
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
    std::ostringstream error;
    error << "Path does not match any known routes: " << path;
    throw http::not_found(error.str());
  }
}
