#ifndef API06_NODES_HANDLER_HPP
#define API06_NODES_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/request.hpp"
#include <string>
#include <list>

namespace api06 {

class nodes_responder : public osm_current_responder {
public:
  nodes_responder(mime::type, std::list<osm_id_t>, factory_ptr &);
  ~nodes_responder();

private:
  std::list<osm_id_t> ids;
};

class nodes_handler : public handler {
public:
  nodes_handler(request &req);
  ~nodes_handler();

  std::string log_name() const;
  responder_ptr_t responder(factory_ptr &x) const;

private:
  std::list<osm_id_t> ids;

  static std::list<osm_id_t> validate_request(request &req);
};

} // namespace api06

#endif /* API06_NODE_HANDLER_HPP */
