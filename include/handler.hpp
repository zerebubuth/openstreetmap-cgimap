#ifndef HANDLER_HPP
#define HANDLER_HPP

#include "types.hpp"
#include "output_formatter.hpp"
#include "mime_types.hpp"
#include "data_selection.hpp"

#include <list>
#include <string>
#include <memory>
#include <boost/shared_ptr.hpp>

/**
 * object which is able to respond to an already-setup request.
 */
class responder {
public:
	 responder(mime::type);
  virtual ~responder();
  virtual void write(boost::shared_ptr<output_formatter> f, const std::string &generator) = 0;

	 mime::type resource_type() const;

	 virtual std::list<mime::type> types_available() const = 0;
	 bool is_available(mime::type) const;

  // quick hack to get "extra" response headers.
  virtual std::string extra_response_headers() const;

private:
	 mime::type mime_type;
};

typedef boost::shared_ptr<responder> responder_ptr_t;

/**
 * object which is able to validate and create responders from
 * requests.
 */
class handler {
public:
	 handler(mime::type default_type = mime::unspecified_type);
  virtual ~handler();
  virtual std::string log_name() const = 0;
  virtual responder_ptr_t responder(data_selection &) const = 0;

	 void set_resource_type(mime::type);

protected:
	 mime::type mime_type;
};

typedef boost::shared_ptr<handler> handler_ptr_t;

#endif /* HANDLER_HPP */
