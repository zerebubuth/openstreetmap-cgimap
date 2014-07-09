#include "config.h"
#include "osm_responder.hpp"

using std::list;
using boost::shared_ptr;

osm_responder::osm_responder(mime::type mt, factory_ptr &f, boost::optional<bbox> b) 
   : responder(mt), sel(f->make_selection()), bounds(b) {
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
  // TODO: is it possible that formatter can be null?
  output_formatter &fmt = *formatter;

  try {
    fmt.start_document(generator);
    if (bounds) {
      fmt.write_bounds(bounds.get());
    }

    // write all selected nodes
    fmt.start_element_type(element_type_node);
    sel->write_nodes(fmt);
    fmt.end_element_type(element_type_node);

    // all selected ways
    fmt.start_element_type(element_type_way);
    sel->write_ways(fmt);
    fmt.end_element_type(element_type_way);

    // all selected relations
    fmt.start_element_type(element_type_relation);
    sel->write_relations(fmt);
    fmt.end_element_type(element_type_relation);
  
  } catch (const std::exception &e) {
    fmt.error(e);
  }

  fmt.end_document();
}

void osm_responder::add_response_header(const std::string &line) {
  extra_headers << line << "\r\n";
}

std::string osm_responder::extra_response_headers() const {
  return extra_headers.str();
}

