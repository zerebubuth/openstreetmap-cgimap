#ifndef API06_NODE_VERSION_HANDLER_HPP
#define API06_NODE_VERSION_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class node_version_responder : public osm_current_responder {
public:
  node_version_responder(mime::type, osm_nwr_id_t, osm_version_t, data_selection &);

private:
  osm_nwr_id_t id;
  osm_nwr_id_t v;
};

class node_version_handler : public handler {
public:
  node_version_handler(request &req, osm_nwr_id_t id, osm_version_t v);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  osm_nwr_id_t id;
  osm_version_t v;
};

} // namespace api06

#endif /* API06_NODE_VERSION_HANDLER_HPP */
