#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <string>
#include <memory>
#include <pqxx/pqxx>
#include "output_formatter.hpp"

/**
 * object which is able to respond to an already-setup request.
 */
class responder {
public:
  virtual ~responder() throw();
  virtual void write(std::auto_ptr<output_formatter> f) = 0;
};

typedef std::auto_ptr<responder> responder_ptr_t;

namespace formats {
enum format_type {
  JSON,
  XML
};
}

/**
 * object which is able to validate and create responders from
 * requests.
 */
class handler {
public:
  virtual ~handler() throw();
  virtual std::string log_name() const = 0;
  virtual responder_ptr_t responder(pqxx::work &) const = 0;
  virtual formats::format_type format() const = 0;
};

typedef std::auto_ptr<handler> handler_ptr_t;

#endif /* HANDLER_HPP */
