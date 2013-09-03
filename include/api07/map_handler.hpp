#ifndef API07_MAP_HANDLER_HPP
#define API07_MAP_HANDLER_HPP

#include "bbox.hpp"
#include "output_formatter.hpp"
#include "handler.hpp"
#include "osm_responder.hpp"
#include "config.h"
#include <fcgiapp.h>
#include <string>

#ifndef ENABLE_API07
#error This file should not be included unless experimental API 0.7 features are enabled.
#endif /* ENABLE_API07 */

namespace api07 {

class map_responder
  : public osm_responder {
public:
	 map_responder(mime::type, bbox, data_selection &);
  ~map_responder();
};

class map_handler 
  : public handler {
public:
	 explicit map_handler(FCGX_Request &request);
	 map_handler(FCGX_Request &request, int tile_id);
	 ~map_handler();
	 std::string log_name() const;
	 responder_ptr_t responder(data_selection &x) const;
	 
private:
	 bbox bounds;
	 
	 static bbox validate_request(FCGX_Request &request);
};

} // namespace api07

#endif /* API07_MAP_HANDLER_HPP */
