#ifndef NODE_HANDLER_HPP
#define NODE_HANDLER_HPP

#include "handler.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>

class node_responder
	: public responder {
public:
	 node_responder(id_t, pqxx::work &);
	 ~node_responder() throw();
	 void write(std::auto_ptr<output_formatter> f);

private:
	 id_t id;
	 pqxx::work &w;

	 void check_visibility();
};

class node_handler 
	: public handler {
public:
	 node_handler(FCGX_Request &request, id_t id);
	 ~node_handler() throw();

	 std::string log_name() const;
	 responder_ptr_t responder(pqxx::work &x) const;
	 formats::format_type format() const;

private:
	 id_t id;
};

#endif /* NODE_HANDLER_HPP */
