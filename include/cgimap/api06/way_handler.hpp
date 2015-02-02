#ifndef API06_WAY_HANDLER_HPP
#define API06_WAY_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class way_responder : public osm_current_responder {
public:
  way_responder(mime::type, osm_id_t, factory_ptr &);
  ~way_responder();

private:
  osm_id_t id;

  void check_visibility();
};

class way_handler : public handler {
public:
  way_handler(request &req, osm_id_t id);
  ~way_handler();

  std::string log_name() const;
  responder_ptr_t responder(factory_ptr &x) const;

private:
  osm_id_t id;
};

} // namespace api06

#endif /* API06_WAY_HANDLER_HPP */
