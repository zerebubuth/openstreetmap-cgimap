#ifndef API06_CHANGESET_UPLOAD_HANDLER_HPP
#define API06_CHANGESET_UPLOAD_HANDLER_HPP

#include <string>
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/osm_diffresult_responder.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class changeset_upload_responder : public osm_diffresult_responder {
public:
  changeset_upload_responder(mime::type, data_update_ptr &, osm_changeset_id_t,
                             const std::string &,
                             boost::optional<osm_user_id_t>);
  ~changeset_upload_responder();

private:
  osm_changeset_id_t id;
  data_update_ptr upd;
};

class changeset_upload_handler : public payload_enabled_handler {
public:
  changeset_upload_handler(request &req, osm_changeset_id_t id);
  ~changeset_upload_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

  responder_ptr_t responder(data_update_ptr &,
			    const std::string &payload,
                            boost::optional<osm_user_id_t> user_id) const;

private:
  osm_changeset_id_t id;
};

} // namespace api06

#endif /* API06_CHANGESET_UPLOAD_HANDLER_HPP */
