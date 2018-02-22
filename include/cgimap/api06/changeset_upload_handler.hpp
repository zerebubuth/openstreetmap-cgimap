#ifndef API06_CHANGESET_UPLOAD_HANDLER_HPP
#define API06_CHANGESET_UPLOAD_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osmchange_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class changeset_upload_responder : public osmchange_responder {
public:
  changeset_upload_responder(mime::type, osm_changeset_id_t, data_selection_ptr &);
  ~changeset_upload_responder();

private:
  osm_changeset_id_t id;
};

class changeset_upload_handler : public handler {
public:
  changeset_upload_handler(request &req, osm_changeset_id_t id);
  ~changeset_upload_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

private:
  osm_changeset_id_t id;
};

} // namespace api06

#endif /* API06_CHANGESET_UPLOAD_HANDLER_HPP */
