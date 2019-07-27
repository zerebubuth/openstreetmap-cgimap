#ifndef API06_CHANGESET_CREATE_HANDLER_HPP
#define API06_CHANGESET_CREATE_HANDLER_HPP

#include <string>

#include "cgimap/text_responder.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class changeset_create_responder : public text_responder {
public:
  changeset_create_responder(mime::type, data_update_ptr &,
                             const std::string &,
                             boost::optional<osm_user_id_t>);
  ~changeset_create_responder();

private:
  data_update_ptr upd;
};

class changeset_create_handler : public payload_enabled_handler {
public:
  changeset_create_handler(request &req);
  ~changeset_create_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

  responder_ptr_t responder(data_update_ptr &,
			    data_selection_ptr & sel,
			    const std::string &payload,
                            boost::optional<osm_user_id_t> user_id) const;

};

} // namespace api06

#endif /* API06_CHANGESET_CREATE_HANDLER_HPP */
