#ifndef API06_WAY_FULL_HANDLER_HPP
#define API06_WAY_FULL_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <string>

namespace api06 {

class way_full_responder
  : public osm_responder {
public:
  way_full_responder(mime::type, id_t, data_selection &);
  ~way_full_responder();

private:
  id_t id;
  
  void check_visibility();
};

class way_full_handler 
  : public handler {
public:
  way_full_handler(FCGX_Request &request, id_t id);
  ~way_full_handler();
  
  std::string log_name() const;
  responder_ptr_t responder(data_selection &x) const;
  
private:
  id_t id;
};

} // namespace api06

#endif /* API06_WAY_FULL_HANDLER_HPP */
