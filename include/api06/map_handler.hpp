#ifndef API06_MAP_HANDLER_HPP
#define API06_MAP_HANDLER_HPP

#include "bbox.hpp"
#include "backend/apidb/cache.hpp"
#include "backend/apidb/changeset.hpp"
#include "output_formatter.hpp"
#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <string>

namespace api06 {

class map_responder
  : public osm_responder {
public:
	 map_responder(mime::type, bbox, data_selection &);
  ~map_responder();
};

class map_handler 
  : public handler {
public:
  map_handler(FCGX_Request &request);
  ~map_handler();
  std::string log_name() const;
  responder_ptr_t responder(data_selection &x) const;

private:
  bbox bounds;

  static bbox validate_request(FCGX_Request &request);
};

} // namespace api06

#endif /* API06_MAP_HANDLER_HPP */
