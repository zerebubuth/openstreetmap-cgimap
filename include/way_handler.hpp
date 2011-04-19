#ifndef WAY_HANDLER_HPP
#define WAY_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>

class way_responder
	: public osm_responder {
public:
	 way_responder(mime::type, id_t, pqxx::work &);
	 ~way_responder() throw();

private:
	 id_t id;

	 void check_visibility();
};

class way_handler 
	: public handler {
public:
	 way_handler(FCGX_Request &request, id_t id);
	 ~way_handler() throw();

	 std::string log_name() const;
	 responder_ptr_t responder(pqxx::work &x) const;

private:
	 id_t id;
};

#endif /* WAY_HANDLER_HPP */
