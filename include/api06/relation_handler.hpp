#ifndef API06_RELATION_HANDLER_HPP
#define API06_RELATION_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <string>

namespace api06 {

class relation_responder
	: public osm_responder {
public:
	 relation_responder(mime::type, id_t, data_selection &);
	 ~relation_responder();

private:
	 id_t id;

	 void check_visibility();
};

class relation_handler 
	: public handler {
public:
	 relation_handler(FCGX_Request &request, id_t id);
	 ~relation_handler();

	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;

private:
	 id_t id;
};

} // namespace api06

#endif /* API06_RELATION_HANDLER_HPP */
