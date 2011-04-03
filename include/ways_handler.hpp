#ifndef WAYS_HANDLER_HPP
#define WAYSS_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "fcgi_helpers.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>
#include <list>

using std::list;

class ways_responder
	: public osm_responder {
public:
	 ways_responder(mime::type, list<id_t>, pqxx::work &);
	 ~ways_responder() throw();

private:
	 list<id_t> ids;
};

class ways_handler 
	: public handler {
public:
	 ways_handler(FCGX_Request &request);
	 ~ways_handler() throw();

	 std::string log_name() const;
	 responder_ptr_t responder(pqxx::work &x) const;

private:
	 list<id_t> ids;

	static list<id_t> validate_request(FCGX_Request &request);
};

#endif /* WAY_HANDLER_HPP */
