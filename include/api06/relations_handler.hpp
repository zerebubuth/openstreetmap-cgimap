#ifndef API06_RELATIONS_HANDLER_HPP
#define API06_RELATIONS_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "request_helpers.hpp"
#include "data_selection.hpp"
#include "request.hpp"
#include <string>
#include <list>

namespace api06 {

class relations_responder
	: public osm_responder {
public:
	 relations_responder(mime::type, std::list<osm_id_t>, data_selection &);
	 ~relations_responder();

private:
	 std::list<osm_id_t> ids;
};

class relations_handler 
	: public handler {
public:
	 relations_handler(request &req);
	 ~relations_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 std::list<osm_id_t> ids;

	 static std::list<osm_id_t> validate_request(request &req);
};

} // namespace api06

#endif /* API06_RELATION_HANDLER_HPP */
