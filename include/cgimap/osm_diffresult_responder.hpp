#ifndef OSM_DIFFRESULT_RESPONDER_HPP
#define OSM_DIFFRESULT_RESPONDER_HPP

#include "cgimap/osm_responder.hpp"
#include "cgimap/api06/changeset_upload/osmchange_tracking.hpp"

/**
 * utility class - inherit from this when implementing something that responds
 * with an diffResult document.
 */
class osm_diffresult_responder : public osm_responder {
public:

  osm_diffresult_responder(mime::type);

  virtual ~osm_diffresult_responder();


  void write(std::shared_ptr<output_formatter> f,
             const std::string &generator,
             const std::chrono::system_clock::time_point &now);

protected:
  std::shared_ptr<api06::OSMChange_Tracking> change_tracking;
};

#endif /* OSM_DIFFRESULT_RESPONDER_HPP */
