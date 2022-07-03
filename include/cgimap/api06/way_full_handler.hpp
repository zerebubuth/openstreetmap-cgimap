#ifndef API06_WAY_FULL_HANDLER_HPP
#define API06_WAY_FULL_HANDLER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/osm_current_responder.hpp"
#include "cgimap/request.hpp"
#include <string>

namespace api06 {

class way_full_responder : public osm_current_responder {
public:
  way_full_responder(mime::type, osm_nwr_id_t, data_selection &);

private:
  osm_nwr_id_t id;

  void check_visibility();
};

class way_full_handler : public handler {
public:
  way_full_handler(request &req, osm_nwr_id_t id);

  std::string log_name() const override;
  responder_ptr_t responder(data_selection &x) const override;

private:
  osm_nwr_id_t id;
};

} // namespace api06

#endif /* API06_WAY_FULL_HANDLER_HPP */
