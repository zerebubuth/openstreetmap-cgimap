#include "routes.hpp"
#include "handler.hpp"
#include "map_handler.hpp"
#include "router.hpp"

using std::auto_ptr;
using std::list;
using std::string;
using boost::shared_ptr;
using boost::fusion::make_cons;
using boost::fusion::invoke;
using boost::ref;

struct router {
  typedef boost::shared_ptr<handler> handler_ptr;

  struct rule_base {
    virtual ~rule_base() {}
    virtual bool invoke_if(const list<string> &, FCGX_Request &, handler_ptr &) = 0;
  };

  typedef boost::shared_ptr<rule_base> rule_ptr;

  template <typename rule_t, typename func_t>
  struct rule : public rule_base {
    //typedef typename boost::function_types::function_type<boost::fusion::cons<bool, typename rule_t::match_type> >::type func_t;
    rule_t r;
    func_t func;
    rule(rule_t r_, func_t f_) : r(r_), func(f_) {}
    bool invoke_if(const list<string> &parts, FCGX_Request &params, handler_ptr &ptr) {
      try {
	list<string>::const_iterator begin = parts.begin();
	ptr.reset(invoke(func,make_cons(ref(params), r.match(begin, parts.end()))));
	return true;
      } catch(const match::error &e) {
	return false;
      }
    }
  };

  template<typename Handler, typename Rule>
  void add(Rule r) {
    boost::factory<Handler*> ctor;
    rules.push_back(rule_ptr(new rule<Rule, boost::factory<Handler*> >(r, ctor)));
  }

  handler_ptr match(const list<string> &p, FCGX_Request &params) {
    handler_ptr hptr;
    for (list<rule_ptr>::iterator itr = rules.begin(); itr != rules.end(); ++itr) {
      rule_ptr rptr = *itr;
      if (rptr->invoke_if(p, params, hptr)) {
	break;
      }
    }
    return hptr;
  }

private:
  list<rule_ptr> rules;
};

handler_ptr_t
route_request(FCGX_Request &request) {
  using match::root_;

  router r;
  std::list<std::string> l;

  r.add<map_handler>(root_ / "api" / "0.6" / "map");

  router::handler_ptr hptr(r.match(l, request));

  return auto_ptr<handler>(new map_handler(request));
}
