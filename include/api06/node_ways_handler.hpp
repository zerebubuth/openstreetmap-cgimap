#ifndef API06_NODE_WAYS_HANDLER_HPP
#define API06_NODE_WAYS_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <string>

namespace api06 {

class node_ways_responder
	: public osm_responder {
public:
	 node_ways_responder(mime::type, osm_id_t, data_selection &);
	 ~node_ways_responder();

private:
	 osm_id_t id;

	 void check_visibility();
};

class node_ways_handler 
	: public handler {
public:
	 node_ways_handler(FCGX_Request &request, osm_id_t id);
	 ~node_ways_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 osm_id_t id;
};

} // namespace api06

#endif /* API06_NODE_WAYS_HANDLER_HPP */
