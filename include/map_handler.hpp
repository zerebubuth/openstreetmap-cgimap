#ifndef MAP_HANDLER_HPP
#define MAP_HANDLER_HPP

#include "bbox.hpp"
#include "cache.hpp"
#include "changeset.hpp"
#include "output_formatter.hpp"
#include "handler.hpp"
#include "osm_responder.hpp"
#include <fcgiapp.h>
#include <pqxx/pqxx>
#include <string>

class map_responder
  : public osm_responder {
public:
	 map_responder(mime::type, bbox, pqxx::work&);
  ~map_responder() throw();
};

class map_handler 
  : public handler {
public:
  map_handler(FCGX_Request &request);
  ~map_handler() throw();
  std::string log_name() const;
  responder_ptr_t responder(pqxx::work &x) const;

private:
  bbox bounds;

  static bbox validate_request(FCGX_Request &request);
};

#endif /* MAP_HANDLER_HPP */
