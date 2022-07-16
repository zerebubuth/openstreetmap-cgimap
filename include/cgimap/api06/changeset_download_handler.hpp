#ifndef API06_CHANGESET_DOWNLOAD_HANDLER_HPP
#define API06_CHANGESET_DOWNLOAD_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osmchange_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class changeset_download_responder : public osmchange_responder {
public:
  changeset_download_responder(mime::type, osm_changeset_id_t, data_selection &);

private:
  osm_changeset_id_t id;
};

class changeset_download_handler : public handler {
public:
  changeset_download_handler(request &req, osm_changeset_id_t id);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  osm_changeset_id_t id;
};

} // namespace api06

#endif /* API06_CHANGESET_DOWNLOAD_HANDLER_HPP */
