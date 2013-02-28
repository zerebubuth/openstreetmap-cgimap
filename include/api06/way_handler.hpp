#ifndef API06_WAY_HANDLER_HPP
#define API06_WAY_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <string>

namespace api06 {

class way_responder
	: public osm_responder {
public:
	 way_responder(mime::type, osm_id_t, data_selection &);
	 ~way_responder();

private:
	 osm_id_t id;

	 void check_visibility();
};

class way_handler 
	: public handler {
public:
	 way_handler(FCGX_Request &request, osm_id_t id);
	 ~way_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 osm_id_t id;
};

} // namespace api06

#endif /* API06_WAY_HANDLER_HPP */
