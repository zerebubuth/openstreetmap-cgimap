#ifndef OSM_RESPONDER_HPP
#define OSM_RESPONDER_HPP

#include "handler.hpp"
#include "bbox.hpp"

#include <pqxx/pqxx>
#include <boost/optional.hpp>

class osm_responder : public responder {
public:
	 osm_responder(mime::type, pqxx::work &, boost::optional<bbox> bounds = boost::optional<bbox>());
	 virtual ~osm_responder() throw();
	 std::list<mime::type> types_available() const;
	 void write(std::auto_ptr<output_formatter> f);
protected:
	 pqxx::work &w;
	 boost::optional<bbox> bounds;
};

#endif /* OSM_RESPONDER_HPP */
