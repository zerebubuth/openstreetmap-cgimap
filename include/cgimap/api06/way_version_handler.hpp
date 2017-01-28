#ifndef API06_WAY_VERSION_HANDLER_HPP
#define API06_WAY_VERSION_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class way_version_responder : public osm_current_responder {
public:
  way_version_responder(mime::type, osm_nwr_id_t, osm_version_t, data_selection_ptr &);
  ~way_version_responder();

private:
  osm_nwr_id_t id;
  osm_version_t v;

  void check_visibility();
};

class way_version_handler : public handler {
public:
  way_version_handler(request &req, osm_nwr_id_t id, osm_version_t v);
  ~way_version_handler();

  std::string log_name() const;
  responder_ptr_t responder(data_selection_ptr &x) const;

private:
  osm_nwr_id_t id;
  osm_version_t v;
};

} // namespace api06

#endif /* API06_WAY_VERSION_HANDLER_HPP */
