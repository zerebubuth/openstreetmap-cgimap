#ifndef DIFFRESULT_RESPONDER_HPP
#define DIFFRESULT_RESPONDER_HPP

#include "cgimap/osm_responder.hpp"

/**
 * utility class - inherit from this when implementing something that responds
 * with a diffresult message (result of an changeset upload operation)
 */
class diffresult_responder : public responder {
public:

  diffresult_responder(mime::type);

  virtual ~diffresult_responder();

  void write(boost::shared_ptr<output_formatter> f,
             const std::string &generator,
             const boost::posix_time::ptime &now);

  mime::type resource_type() const;

protected:
//  // selection of elements to be written out
//  data_selection_ptr sel;
};

#endif /* DIFFRESULT_RESPONDER_HPP */
