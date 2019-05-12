#ifndef TEXT_RESPONDER_HPP
#define TEXT_RESPONDER_HPP

#include "cgimap/handler.hpp"
#include "cgimap/bbox.hpp"
#include "cgimap/data_selection.hpp"

#include <boost/optional.hpp>
#include <sstream>

/**
 * utility class - use this as a base class when the derived class is going to
 * respond with a text response
 */
class text_responder : public responder {
public:
  // construct, passing the mime type down to the responder.
  // optional bounds are stored at this level, but available to derived classes.
  text_responder(mime::type);

  virtual ~text_responder();

  // lists the standard types that OSM format can respond in, currently XML and,
  // if
  // the yajl library is provided, JSON.
  virtual std::list<mime::type> types_available() const;

  // quick hack to add headers to the response
  std::string extra_response_headers() const;

  void write(std::shared_ptr<output_formatter> f,
             const std::string &generator,
             const std::chrono::system_clock::time_point &now);

protected:

  // quick hack to provide extra response headers like Content-Disposition.
  std::ostringstream extra_headers;

  // adds an extra response header.
  void add_response_header(const std::string &);

  std::string output_text;
};

#endif /* TEXT_RESPONDER_HPP */
