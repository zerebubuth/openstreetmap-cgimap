#ifndef OSM_CURRENT_RESPONDER_HPP
#define OSM_CURRENT_RESPONDER_HPP

#include "cgimap/osm_responder.hpp"

/**
 * utility class - inherit from this when implementing somthing
 * responding with OSM data from the current tables.
 */
class osm_current_responder : public osm_responder {
public:
  // construct, passing the mime type down to the responder.
  // optional bounds are stored at this level, but available to derived classes.
  osm_current_responder(mime::type, data_selection_ptr &s,
                        boost::optional<bbox> bounds = boost::optional<bbox>());

  virtual ~osm_current_responder();

  // writes whatever is in the tmp_nodes/ways/relations tables to the given
  // formatter.
  void write(boost::shared_ptr<output_formatter> f,
             const std::string &generator,
             const boost::posix_time::ptime &now);

protected:
  // current selection of elements to be written out
  data_selection_ptr sel;
};

#endif /* OSM_CURRENT_RESPONDER_HPP */
