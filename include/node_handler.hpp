#ifndef NODE_HANDLER_HPP
#define NODE_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>

class node_responder
	: public osm_responder {
public:
	 node_responder(mime::type, id_t, pqxx::work &);
	 ~node_responder() throw();

private:
	 id_t id;

	 void check_visibility();
};

class node_handler 
	: public handler {
public:
	 node_handler(FCGX_Request &request, id_t id);
	 ~node_handler() throw();

	 std::string log_name() const;
	 responder_ptr_t responder(pqxx::work &x) const;

private:
	 id_t id;
};

#endif /* NODE_HANDLER_HPP */
