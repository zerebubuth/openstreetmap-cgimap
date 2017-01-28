#ifndef API06_NODE_HISTORY_HANDLER_HPP
#define API06_NODE_HISTORY_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class node_history_responder : public osm_current_responder {
public:
  node_history_responder(mime::type, osm_nwr_id_t, data_selection_ptr &);
  ~node_history_responder();

private:
  osm_nwr_id_t id;
};

class node_history_handler : public handler {
public:
  node_history_handler(request &, osm_nwr_id_t);
  ~node_history_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &) const;

private:
  osm_nwr_id_t id;
};

} // namespace api06

#endif /* API06_NODE_HISTORY_HANDLER_HPP */
