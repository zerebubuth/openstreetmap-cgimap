#ifndef API06_NODE_RELATIONS_HANDLER_HPP
#define API06_NODE_RELATIONS_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class node_relations_responder : public osm_current_responder {
public:
  node_relations_responder(mime::type, osm_nwr_id_t, data_selection &);

private:
  osm_nwr_id_t id;

  bool is_visible();
};

class node_relations_handler : public handler {
public:
  node_relations_handler(request &req, osm_nwr_id_t id);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  osm_nwr_id_t id;
};

} // namespace api06

#endif /* API06_NODE_RELATIONS_HANDLER_HPP */
