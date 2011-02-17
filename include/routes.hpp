#ifndef ROUTES_HPP
#define ROUTES_HPP

#include "handler.hpp"
#include <fcgiapp.h>
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>

// internal implementation of the routes
struct router;

/**
 * encapsulates routing (URL to handler mapping) information, similar in 
 * intent, if not form, to rails' routes.rb.
 */
class routes 
	: public boost::noncopyable {
public:
	 routes();
	 ~routes();
	 
	 /**
		* returns the handler which matches a request, or throws a 404 error.
		*/
	 handler_ptr_t operator()(FCGX_Request &request) const;

private:
	 // common prefix of all routes
	 std::string common_prefix;

	 // object which actually does the routing.
	 boost::scoped_ptr<router> r;
};

#endif /* ROUTES_HPP */
