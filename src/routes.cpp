#include "routes.hpp"
#include "handler.hpp"
#include "map_handler.hpp"
#include "node_handler.hpp"
#include "router.hpp"
#include "fcgi_helpers.hpp"
#include "http.hpp"
#include <boost/algorithm/string.hpp>

using std::auto_ptr;
using std::list;
using std::string;
using boost::shared_ptr;
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
	: r(new router()), common_prefix("api/0.6/") {
  using match::root_;
  using match::int_;

	r->add<map_handler>(root_ / "map");
	r->add<node_handler>(root_ / "node" / int_);
}

routes::~routes() {
}

handler_ptr_t
routes::operator()(FCGX_Request &request) const {
	// full path from request handler
	string path = get_request_path(request);

	// check the prefix
	if (path.compare(0, common_prefix.size(), common_prefix) == 0) {
		string suffix(path, common_prefix.size());
		list<string> path_components;
		al::split(path_components, suffix, al::is_any_of("/"));

		handler_ptr_t hptr(r->match(path_components, request));
		
		// if the pointer points at something, then the path was found. otherwise,
		// it must have exhausted all the possible routes.
		if (hptr) {
			return hptr;
		}
	}

	// doesn't match prefix...
	throw http::not_found(path);
}
