#ifndef API06_NODES_HANDLER_HPP
#define API06_NODES_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "fcgi_helpers.hpp"
#include <fcgiapp.h>
#include <string>
#include <list>

namespace api06 {

class nodes_responder
	: public osm_responder {
public:
	 nodes_responder(mime::type, std::list<id_t>, data_selection &);
	 ~nodes_responder();

private:
	 std::list<id_t> ids;
};

class nodes_handler 
	: public handler {
public:
	 nodes_handler(FCGX_Request &request);
	 ~nodes_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 std::list<id_t> ids;

	 static std::list<id_t> validate_request(FCGX_Request &request);
};

} // namespace api06

#endif /* API06_NODE_HANDLER_HPP */
