#ifndef RELATION_FULL_HANDLER_HPP
#define RELATION_FULL_HANDLER_HPP

#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <string>

class relation_full_responder
  : public osm_responder {
public:
  relation_full_responder(mime::type, id_t, data_selection &);
  ~relation_full_responder();

private:
  id_t id;
  
  void check_visibility();
};

class relation_full_handler 
  : public handler {
public:
  relation_full_handler(FCGX_Request &request, id_t id);
  ~relation_full_handler();
  
  std::string log_name() const;
  responder_ptr_t responder(data_selection &x) const;
  
private:
  id_t id;
};

#endif /* RELATION_FULL_HANDLER_HPP */
