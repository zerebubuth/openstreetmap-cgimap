#ifndef API06_CHANGESET_HANDLER_HPP
#define API06_CHANGESET_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class changeset_responder : public osm_current_responder {
public:
  changeset_responder(mime::type, osm_changeset_id_t, bool, data_selection_ptr &);
  ~changeset_responder();

private:
  osm_changeset_id_t id;
  bool include_discussion;
};

class changeset_handler : public handler {
public:
  changeset_handler(request &req, osm_changeset_id_t id);
  ~changeset_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

private:
  osm_changeset_id_t id;
  bool include_discussion;
};

} // namespace api06

#endif /* API06_CHANGESET_HANDLER_HPP */
