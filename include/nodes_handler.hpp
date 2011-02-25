#ifndef NODES_HANDLER_HPP
#define NODES_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "fcgi_helpers.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>
#include <list>

using std::list;

class nodes_responder
	: public osm_responder {
public:
	 nodes_responder(mime::type, list<id_t>, pqxx::work &);
	 ~nodes_responder() throw();

private:
	 list<id_t> ids;
};

class nodes_handler 
	: public handler {
public:
	 nodes_handler(FCGX_Request &request);
	 ~nodes_handler() throw();

	 std::string log_name() const;
	 responder_ptr_t responder(pqxx::work &x) const;

private:
	 list<id_t> ids;

	static list<id_t> validate_request(FCGX_Request &request);
};

#endif /* NODE_HANDLER_HPP */
