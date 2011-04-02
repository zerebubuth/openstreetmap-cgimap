#ifndef HANDLER_HPP
#define HANDLER_HPP

#include "output_formatter.hpp"
#include "mime_types.hpp"

#include <list>
#include <string>
#include <memory>
#include <pqxx/pqxx>
#include <boost/shared_ptr.hpp>

/**
 * object which is able to respond to an already-setup request.
 */
class responder {
public:
	 responder(mime::type);
  virtual ~responder() throw();
  virtual void write(boost::shared_ptr<output_formatter> f) = 0;

	 mime::type resource_type() const;

	 virtual std::list<mime::type> types_available() const = 0;
	 bool is_available(mime::type) const;

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
  virtual ~handler() throw();
  virtual std::string log_name() const = 0;
  virtual responder_ptr_t responder(pqxx::work &) const = 0;

	 void set_resource_type(mime::type);

protected:
	 mime::type mime_type;
};

typedef boost::shared_ptr<handler> handler_ptr_t;

#endif /* HANDLER_HPP */
