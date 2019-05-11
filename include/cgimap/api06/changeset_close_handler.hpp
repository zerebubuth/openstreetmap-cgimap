#ifndef API06_CHANGESET_CLOSE_HANDLER_HPP
#define API06_CHANGESET_CLOSE_HANDLER_HPP

#include <string>
#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"
#include "cgimap/empty_responder.hpp"

namespace api06 {

class changeset_close_responder : public empty_responder {
public:
  changeset_close_responder(mime::type, data_update_ptr &, osm_changeset_id_t,
                             const std::string &,
                             boost::optional<osm_user_id_t>);
  ~changeset_close_responder();

private:
  data_update_ptr upd;
};

class changeset_close_handler : public payload_enabled_handler {
public:
  changeset_close_handler(request &req, osm_changeset_id_t id);
  ~changeset_close_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

  responder_ptr_t responder(data_update_ptr &,
			    const std::string &payload,
                            boost::optional<osm_user_id_t> user_id) const;

private:
  osm_changeset_id_t id;
};

} // namespace api06

#endif /* API06_CHANGESET_CLOSE_HANDLER_HPP */
