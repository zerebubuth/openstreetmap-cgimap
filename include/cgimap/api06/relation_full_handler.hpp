#ifndef API06_RELATION_FULL_HANDLER_HPP
#define API06_RELATION_FULL_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class relation_full_responder : public osm_current_responder {
public:
  relation_full_responder(mime::type, osm_id_t, factory_ptr &);
  ~relation_full_responder();

private:
  osm_id_t id;

  void check_visibility();
};

class relation_full_handler : public handler {
public:
  relation_full_handler(request &req, osm_id_t id);
  ~relation_full_handler();

  std::string log_name() const;
  responder_ptr_t responder(factory_ptr &x) const;

private:
  osm_id_t id;
};

} // namespace api06

#endif /* API06_RELATION_FULL_HANDLER_HPP */
