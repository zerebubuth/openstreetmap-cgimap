#include "cgimap/config.hpp"
#include "cgimap/osm_current_responder.hpp"

using std::list;
using boost::shared_ptr;

osm_current_responder::osm_current_responder(mime::type mt, factory_ptr &f,
                                             boost::optional<bbox> b)
    : osm_responder(mt, b), sel(f->make_selection()) {}

osm_current_responder::~osm_current_responder() {}

void osm_current_responder::write(shared_ptr<output_formatter> formatter,
                                  const std::string &generator) {
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
