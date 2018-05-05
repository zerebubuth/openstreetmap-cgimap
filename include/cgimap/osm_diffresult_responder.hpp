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


  void write(boost::shared_ptr<output_formatter> f,
             const std::string &generator,
             const boost::posix_time::ptime &now);

protected:
  std::shared_ptr<OSMChange_Tracking> change_tracking;
};

#endif /* OSM_DIFFRESULT_RESPONDER_HPP */
