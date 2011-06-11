#ifndef API06_NODE_HANDLER_HPP
#define API06_NODE_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <string>

namespace api06 {

class node_responder
	: public osm_responder {
public:
	 node_responder(mime::type, id_t, data_selection &);
	 ~node_responder();

private:
	 id_t id;

	 void check_visibility();
};

class node_handler 
	: public handler {
public:
	 node_handler(FCGX_Request &request, id_t id);
	 ~node_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 id_t id;
};

} // namespace api06

#endif /* API06_NODE_HANDLER_HPP */
