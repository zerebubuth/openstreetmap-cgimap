#include "config.h"
#include "osm_responder.hpp"

using std::list;
using boost::shared_ptr;

osm_responder::osm_responder(mime::type mt, data_selection &s, boost::optional<bbox> b) 
  : responder(mt), sel(s), bounds(b) {
}

osm_responder::~osm_responder() {
}

list<mime::type> 
osm_responder::types_available() const {
  list<mime::type> types;
  types.push_back(mime::text_xml);
#ifdef HAVE_YAJL
  types.push_back(mime::text_json);
#endif
  return types;
}

void
osm_responder::write(shared_ptr<output_formatter> formatter, const std::string &generator) {
  try {
    formatter->start_document(generator);
    if (bounds) {
      formatter->write_bounds(bounds.get());
    }

    sel.write_nodes(*formatter);
    sel.write_ways(*formatter);
    sel.write_relations(*formatter);
  
  } catch (const std::exception &e) {
    formatter->error(e);
  }

  formatter->end_document();
}

void osm_responder::add_response_header(const std::string &line) {
  extra_headers << line << "\r\n";
}

std::string osm_responder::extra_response_headers() const {
  return extra_headers.str();
}

