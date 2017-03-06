#ifndef API06_MAP_HANDLER_HPP
#define API06_MAP_HANDLER_HPP

#include "cgimap/bbox.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class map_responder : public osm_current_responder {
public:
  map_responder(mime::type, bbox, data_selection_ptr &);
  ~map_responder();
};

class map_handler : public handler {
public:
  map_handler(request &req);
  ~map_handler();
  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

private:
  bbox bounds;

  static bbox validate_request(request &req);
};

} // namespace api06

#endif /* API06_MAP_HANDLER_HPP */
