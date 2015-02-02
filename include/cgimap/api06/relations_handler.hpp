#ifndef API06_RELATIONS_HANDLER_HPP
#define API06_RELATIONS_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/data_selection.hpp"
#include "cgimap/request.hpp"
#include <string>
#include <list>

namespace api06 {

class relations_responder : public osm_current_responder {
public:
  relations_responder(mime::type, std::list<osm_id_t>, factory_ptr &);
  ~relations_responder();

private:
  std::list<osm_id_t> ids;
};

class relations_handler : public handler {
public:
  relations_handler(request &req);
  ~relations_handler();

  std::string log_name() const;
  responder_ptr_t responder(factory_ptr &x) const;

private:
  std::list<osm_id_t> ids;

  static std::list<osm_id_t> validate_request(request &req);
};

} // namespace api06

#endif /* API06_RELATION_HANDLER_HPP */
