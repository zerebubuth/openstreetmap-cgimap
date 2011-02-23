#ifndef NODES_HANDLER_HPP
#define NODES_HANDLER_HPP

#include "handler.hpp"
#include "fcgi_helpers.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>
#include <list>

using std::list;


class nodes_responder
	: public responder {
public:
	 nodes_responder(list<id_t>, pqxx::work &);
	 ~nodes_responder() throw();
	 void write(std::auto_ptr<output_formatter> f);

private:
	 list<id_t> ids;
	 pqxx::work &w;

};

class nodes_handler 
	: public handler {
public:
	 nodes_handler(FCGX_Request &request);
	 ~nodes_handler() throw();

	 std::string log_name() const;
	 responder_ptr_t responder(pqxx::work &x) const;
	 formats::format_type format() const;

private:
	 list<id_t> ids;

	static list<id_t> validate_request(FCGX_Request &request);
};

#endif /* NODE_HANDLER_HPP */
