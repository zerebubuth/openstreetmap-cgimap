#ifndef WAYS_HANDLER_HPP
#define WAYSS_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "fcgi_helpers.hpp"
#include <fcgiapp.h>
#include <string>
#include <list>

using std::list;

class ways_responder
	: public osm_responder {
public:
	 ways_responder(mime::type, list<id_t>, data_selection &);
	 ~ways_responder();

private:
	 list<id_t> ids;
};

class ways_handler 
	: public handler {
public:
	 ways_handler(FCGX_Request &request);
	 ~ways_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 list<id_t> ids;

	static list<id_t> validate_request(FCGX_Request &request);
};

#endif /* WAY_HANDLER_HPP */
