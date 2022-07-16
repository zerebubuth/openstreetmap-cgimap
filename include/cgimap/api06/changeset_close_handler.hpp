#ifndef API06_CHANGESET_CLOSE_HANDLER_HPP
#define API06_CHANGESET_CLOSE_HANDLER_HPP

#include <string>

#include "cgimap/text_responder.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class changeset_close_responder : public text_responder {
public:
  changeset_close_responder(mime::type, data_update &, osm_changeset_id_t,
                             const std::string &,
                             std::optional<osm_user_id_t>);
};

class changeset_close_handler : public payload_enabled_handler {
public:
  changeset_close_handler(request &req, osm_changeset_id_t id);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

  responder_ptr_t responder(data_update &,
			    const std::string &payload,
                            std::optional<osm_user_id_t> user_id) const override;
  bool requires_selection_after_update() const override;

private:
  osm_changeset_id_t id;
};

} // namespace api06

#endif /* API06_CHANGESET_CLOSE_HANDLER_HPP */
