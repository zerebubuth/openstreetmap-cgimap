#ifndef OSMCHANGE_RESPONDER_HPP
#define OSMCHANGE_RESPONDER_HPP

#include "cgimap/osm_responder.hpp"

/**
 * utility class - inherit from this when implementing something that responds
 * with an osmChange (a.k.a "diff") document.
 */
class osmchange_responder : public osm_responder {
public:
  // construct, passing the mime type down to the responder.
  // optional bounds are stored at this level, but available to derived classes.
  osmchange_responder(mime::type, data_selection_ptr &s);

  virtual ~osmchange_responder();

  // takes the stuff in the tmp_nodes/ways/relations tables and sorts them by
  // timestamp, then wraps them in <create>/<modify>/<delete> to create an
  // approximation of a diff. the reliance on timestamp means it's entirely
  // likely that some documents may be poorly formed.
  void write(boost::shared_ptr<output_formatter> f,
             const std::string &generator,
             const boost::posix_time::ptime &now);

protected:
  // selection of elements to be written out
  data_selection_ptr sel;
};

#endif /* OSMCHANGE_RESPONDER_HPP */
