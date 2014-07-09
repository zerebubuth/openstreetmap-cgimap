#ifndef API06_MAP_HANDLER_HPP
#define API06_MAP_HANDLER_HPP

#include "bbox.hpp"
#include "output_formatter.hpp"
#include "handler.hpp"
#include "osm_current_responder.hpp"
#include "request.hpp"
#include <string>

namespace api06 {

class map_responder
  : public osm_current_responder {
public:
	 map_responder(mime::type, bbox, factory_ptr &);
  ~map_responder();
};

class map_handler 
  : public handler {
public:
  map_handler(request &req);
  ~map_handler();
  std::string log_name() const;
  responder_ptr_t responder(factory_ptr &x) const;

private:
  bbox bounds;

  static bbox validate_request(request &req);
};

} // namespace api06

#endif /* API06_MAP_HANDLER_HPP */
