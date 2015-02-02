#ifndef OSM_RESPONDER_HPP
#define OSM_RESPONDER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/bbox.hpp"
#include "cgimap/data_selection.hpp"

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <sstream>

/**
 * utility class - use this as a base class when the derived class is going to
 * respond in OSM format (i.e: nodes, ways and relations). this class will take
 * care of the write() and types_available() methods, allowing derived code to
 * be more concise.
 *
 * if you want a <bounds> element to be written, include the bounds constructor
 * argument. otherwise leave it out and it'll default to no bounds element.
 */
class osm_responder : public responder {
public:
  // construct, passing the mime type down to the responder.
  // optional bounds are stored at this level, but available to derived classes.
  osm_responder(mime::type,
                boost::optional<bbox> bounds = boost::optional<bbox>());

  virtual ~osm_responder();

  // lists the standard types that OSM format can respond in, currently XML and,
  // if
  // the yajl library is provided, JSON.
  std::list<mime::type> types_available() const;

  // quick hack to add headers to the response
  std::string extra_response_headers() const;

protected:
  // optional bounds element - this is only for information and has no effect on
  // behaviour other than whether the bounds element gets written.
  boost::optional<bbox> bounds;

  // quick hack to provide extra response headers like Content-Disposition.
  std::ostringstream extra_headers;

  // adds an extra response header.
  void add_response_header(const std::string &);
};

#endif /* OSM_RESPONDER_HPP */
