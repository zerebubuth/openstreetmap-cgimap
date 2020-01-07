#ifndef API06_CHANGESET_UPDATE_HANDLER_HPP
#define API06_CHANGESET_UPDATE_HANDLER_HPP

#include <string>

#include "cgimap/text_responder.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/handler.hpp"
#include "cgimap/request.hpp"

namespace api06 {

class changeset_update_responder : public text_responder {
public:
  changeset_update_responder(mime::type,
			     data_update_ptr &,
			     osm_changeset_id_t id_,
                             const std::string & payload,
			     boost::optional<osm_user_id_t> user_id);
  ~changeset_update_responder();

private:
  osm_changeset_id_t id;
};

class changeset_update_sel_responder : public osm_current_responder {
public:
  changeset_update_sel_responder(mime::type,
			     data_selection_ptr & sel,
			     osm_changeset_id_t id_);
  ~changeset_update_sel_responder();

private:
  data_selection_ptr sel;
  osm_changeset_id_t id;
  bool include_discussion;
};

class changeset_update_handler : public payload_enabled_handler {
public:
  changeset_update_handler(request &req, osm_changeset_id_t id);
  ~changeset_update_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

  responder_ptr_t responder(data_update_ptr &,
			    const std::string &payload,
                            boost::optional<osm_user_id_t> user_id) const;
  bool requires_selection_after_update() const;

private:
  osm_changeset_id_t id;
};

} // namespace api06

#endif /* API06_CHANGESET_UPDATE_HANDLER_HPP */
