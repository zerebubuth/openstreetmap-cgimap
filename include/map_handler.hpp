#ifndef MAP_HANDLER_HPP
#define MAP_HANDLER_HPP

#include "writer.hpp"
#include "bbox.hpp"
#include "cache.hpp"
#include "changeset.hpp"
#include "output_formatter.hpp"
#include "handler.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>

class map_responder
  : public responder {
public:
  map_responder(bbox, pqxx::work&);
  ~map_responder() throw();
  void write(std::auto_ptr<output_formatter> f);

private:
  bbox bounds;
  pqxx::work &w;
  static void write_map(pqxx::work &w, output_formatter &formatter, const bbox &bounds);
};

class map_handler 
  : public handler {
public:
  map_handler(FCGX_Request &request);
  ~map_handler() throw();
  std::string log_name() const;
  responder_ptr_t responder(pqxx::work &x) const;
  formats::format_type format() const;

private:
  bbox bounds;
  static bbox validate_request(FCGX_Request &request);
};

#endif /* MAP_HANDLER_HPP */
