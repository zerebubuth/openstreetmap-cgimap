#ifndef API06_WAYS_HANDLER_HPP
#define API06_WAYS_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include "request_helpers.hpp"
#include "request.hpp"
#include <string>
#include <list>

namespace api06 {

class ways_responder
	: public osm_responder {
public:
	 ways_responder(mime::type, std::list<osm_id_t>, data_selection &);
	 ~ways_responder();

private:
	 std::list<osm_id_t> ids;
};

class ways_handler 
	: public handler {
public:
	 ways_handler(request &req);
	 ~ways_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 std::list<osm_id_t> ids;

	 static std::list<osm_id_t> validate_request(request &req);
};

} // namespace api06

#endif /* API06_WAYS_HANDLER_HPP */
