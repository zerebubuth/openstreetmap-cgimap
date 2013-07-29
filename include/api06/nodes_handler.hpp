#ifndef API06_NODES_HANDLER_HPP
#define API06_NODES_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "fcgi_helpers.hpp"
#include "request.hpp"
#include <string>
#include <list>

namespace api06 {

class nodes_responder
	: public osm_responder {
public:
	 nodes_responder(mime::type, std::list<osm_id_t>, data_selection &);
	 ~nodes_responder();

private:
	 std::list<osm_id_t> ids;
};

class nodes_handler 
	: public handler {
public:
	 nodes_handler(request &req);
	 ~nodes_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 std::list<osm_id_t> ids;

	 static std::list<osm_id_t> validate_request(request &req);
};

} // namespace api06

#endif /* API06_NODE_HANDLER_HPP */
