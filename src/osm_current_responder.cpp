#include "cgimap/config.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/osm_current_responder.hpp"

#include <boost/format.hpp>

using std::list;
using std::shared_ptr;

osm_current_responder::osm_current_responder(mime::type mt, data_selection_ptr &s,
                                             boost::optional<bbox> b)
    : osm_responder(mt, b), sel(s) {}

osm_current_responder::~osm_current_responder() = default;

void osm_current_responder::write(shared_ptr<output_formatter> formatter,
                                  const std::string &generator,
                                  const std::chrono::system_clock::time_point &now) {
  // TODO: is it possible that formatter can be null?
  output_formatter &fmt = *formatter;

  try {
    fmt.start_document(generator, "osm");
    if (bounds) {
      fmt.write_bounds(bounds.get());
    }

    // write all selected changesets
    fmt.start_element_type(element_type_changeset);
    sel->write_changesets(fmt, now);
    fmt.end_element_type(element_type_changeset);

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
    logger::message(boost::format("Caught error in osm_current_responder: %1%") %
                      e.what());
    fmt.error(e);
  }

  fmt.end_document();
}
