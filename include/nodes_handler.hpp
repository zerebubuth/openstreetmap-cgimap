#ifndef NODES_HANDLER_HPP
#define NODES_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "fcgi_helpers.hpp"
#include <fcgiapp.h>
#include <string>
#include <list>

using std::list;

class nodes_responder
	: public osm_responder {
public:
	 nodes_responder(mime::type, list<id_t>, data_selection &);
	 ~nodes_responder();

private:
	 list<id_t> ids;
};

class nodes_handler 
	: public handler {
public:
	 nodes_handler(FCGX_Request &request);
	 ~nodes_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 list<id_t> ids;

	static list<id_t> validate_request(FCGX_Request &request);
};

#endif /* NODE_HANDLER_HPP */
