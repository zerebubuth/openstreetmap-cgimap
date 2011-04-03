#include "routes.hpp"
#include "handler.hpp"
#include "map_handler.hpp"
#include "node_handler.hpp"
#include "nodes_handler.hpp"
#include "ways_handler.hpp"
#include "way_full_handler.hpp"
#include "relations_handler.hpp"
#include "relation_full_handler.hpp"
#include "way_handler.hpp"
#include "router.hpp"
#include "fcgi_helpers.hpp"
#include "http.hpp"
#include "mime_types.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>

using std::auto_ptr;
using std::list;
using std::string;
using std::pair;
using boost::shared_ptr;
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
			virtual bool invoke_if(const list<string> &, FCGX_Request &, handler_ptr_t &) = 0;
	 };

	 typedef boost::shared_ptr<rule_base> rule_ptr;

	 // concrete rule match / constructor class
	 template <typename rule_t, typename func_t>
	 struct rule : public rule_base {
			// the DSL rule expression to match
			rule_t r;

			// the function to call (used later as constructor factory)
			func_t func;
			
			rule(rule_t r_, func_t f_) : r(r_), func(f_) {}
			
			// try to match the expression. if it succeeds, call the provided function
			// with the provided params and the matched DSL arguments.
			bool invoke_if(const list<string> &parts, FCGX_Request &params, handler_ptr_t &ptr) {
				try {
					list<string>::const_iterator begin = parts.begin();
					ptr.reset(invoke(func,make_cons(ref(params), r.match(begin, parts.end()))));
					return true;
				} catch(const match::error &e) {
					return false;
				}
			}
	 };
	 
	 /* add a rule (a DSL expression) which constructs the Handler type when the rule
		* matches. the Rule type can be inferred, so generally this can be written as
		* r.add<Handler>(...);
		*/
	 template<typename Handler, typename Rule>
	 void add(Rule r) {
		 // functor to create Handler instances
		 boost::factory<Handler*> ctor;
		 rules.push_back(rule_ptr(new rule<Rule, boost::factory<Handler*> >(r, ctor)));
	 }

	 /* match the list of path components given in p. if a match is found, construct an
		* object of the handler type with the provided params and the matched params.
		*/
	 handler_ptr_t match(const list<string> &p, FCGX_Request &params) {
    handler_ptr_t hptr;
		// it probably isn't necessary to have any more sophisticated data structure
		// than a list at this point. also means the semantics for rule matching are
		// pretty clear - the first match wins.
    for (list<rule_ptr>::iterator itr = rules.begin(); itr != rules.end(); ++itr) {
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
	: r(new router()), common_prefix("/api/0.6/") {
  using match::root_;
  using match::int_;

	r->add<map_handler>(root_ / "map");
	r->add<nodes_handler>(root_ / "nodes");
	r->add<node_handler>(root_ / "node" / int_);
	r->add<ways_handler>(root_ / "ways");
	r->add<way_full_handler>(root_ / "way" / int_ / "full");
	r->add<relations_handler>(root_ / "relations");
	r->add<relation_full_handler>(root_ / "relation" / int_ / "full");
	r->add<way_handler>(root_ / "way" / int_);
}

routes::~routes() {
}


namespace {
/**
 * figures out the mime type from the path specification, e.g: a resource ending 
 * in .xml should be text/xml, .json should be text/json, etc...
 */
pair<string, mime::type> resource_mime_type(const string &path) {
	return make_pair(path, mime::unspecified_type);
}
}

handler_ptr_t
routes::operator()(FCGX_Request &request) const {
	// full path from request handler
	string path = get_request_path(request);

	// check the prefix
	if (path.compare(0, common_prefix.size(), common_prefix) == 0) {
		// strip off the format-spec, if there is one
		pair<string, mime::type> resource = resource_mime_type(string(path, common_prefix.size()));

		// split the URL into bits to be matched.
		list<string> path_components;
		al::split(path_components, resource.first, al::is_any_of("/"));

		handler_ptr_t hptr(r->match(path_components, request));

		// if the pointer points at something, then the path was found. otherwise,
		// it must have exhausted all the possible routes.
		if (hptr) {
			// ugly hack - need this info later on to choose the output formatter,
			// but don't want to parse the URI again...
			hptr->set_resource_type(resource.second);
			
			return hptr;
		}
	}

	// doesn't match prefix...
	throw http::not_found(path);
}
