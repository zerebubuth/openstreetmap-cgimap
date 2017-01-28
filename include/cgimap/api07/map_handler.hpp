#ifndef API07_MAP_HANDLER_HPP
#define API07_MAP_HANDLER_HPP

#include "cgimap/bbox.hpp"
#include "cgimap/output_formatter.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/config.hpp"
#include "cgimap/request.hpp"
#include <string>

#ifndef ENABLE_API07
#error This file should not be included unless experimental API 0.7 features are enabled.
#endif /* ENABLE_API07 */

namespace api07 {

class map_responder : public osm_current_responder {
public:
  map_responder(mime::type, bbox, data_selection_ptr &);
  ~map_responder();
};

class map_handler : public handler {
public:
  explicit map_handler(request &req);
  map_handler(request &req, int tile_id);
  ~map_handler();
  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

private:
  bbox bounds;

  static bbox validate_request(request &req);
};

} // namespace api07

#endif /* API07_MAP_HANDLER_HPP */
