#ifndef API06_WAY_FULL_HANDLER_HPP
#define API06_WAY_FULL_HANDLER_HPP

#include "handler.hpp"
#include "osm_current_responder.hpp"
#include "request.hpp"
#include <string>

namespace api06 {

class way_full_responder
  : public osm_current_responder {
public:
  way_full_responder(mime::type, osm_id_t, factory_ptr &);
  ~way_full_responder();

private:
  osm_id_t id;
  
  void check_visibility();
};

class way_full_handler 
  : public handler {
public:
  way_full_handler(request &req, osm_id_t id);
  ~way_full_handler();
  
  std::string log_name() const;
  responder_ptr_t responder(factory_ptr &x) const;
  
private:
  osm_id_t id;
};

} // namespace api06

#endif /* API06_WAY_FULL_HANDLER_HPP */
