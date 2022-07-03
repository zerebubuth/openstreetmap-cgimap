#ifndef API06_NODES_HANDLER_HPP
#define API06_NODES_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/request.hpp"
#include "cgimap/api06/id_version.hpp"
#include <string>
#include <list>

namespace api06 {

class nodes_responder : public osm_current_responder {
public:
  nodes_responder(mime::type, std::vector<id_version>, data_selection &);

private:
  std::vector<id_version> ids;
};

class nodes_handler : public handler {
public:
  nodes_handler(request &req);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  std::vector<id_version> ids;

  static std::vector<id_version> validate_request(request &req);
};

} // namespace api06

#endif /* API06_NODE_HANDLER_HPP */
