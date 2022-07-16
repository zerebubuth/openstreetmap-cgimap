#ifndef API06_CHANGESET_CREATE_HANDLER_HPP
#define API06_CHANGESET_CREATE_HANDLER_HPP

#include <string>

#include "cgimap/text_responder.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class changeset_create_responder : public text_responder {
public:
  changeset_create_responder(mime::type, data_update &,
                             const std::string &,
                             std::optional<osm_user_id_t>);
};

class changeset_create_handler : public payload_enabled_handler {
public:
  changeset_create_handler(request &req);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

  responder_ptr_t responder(data_update &,
			    const std::string &payload,
                            std::optional<osm_user_id_t> user_id) const override;
  bool requires_selection_after_update() const override;
};

} // namespace api06

#endif /* API06_CHANGESET_CREATE_HANDLER_HPP */
