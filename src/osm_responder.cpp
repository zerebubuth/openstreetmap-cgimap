#include "osm_responder.hpp"
#include "osm_helpers.hpp"

using std::list;
using boost::shared_ptr;

osm_responder::osm_responder(mime::type mt, pqxx::work &x, boost::optional<bbox> b) 
	: responder(mt), w(x), bounds(b) {
}

osm_responder::~osm_responder() throw() {
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
osm_responder::write(shared_ptr<output_formatter> formatter) {
  try {
    formatter->start_document();
		if (bounds) {
			formatter->write_bounds(bounds.get());
		}

    int num_nodes = osm_helpers::num_nodes(w);
    int num_ways = osm_helpers::num_ways(w);
    int num_relations = osm_helpers::num_relations(w);

    if (num_nodes > 0)     osm_helpers::write_tmp_nodes(w, *formatter, num_nodes);
    if (num_ways > 0)      osm_helpers::write_tmp_ways(w, *formatter, num_ways);
    if (num_relations > 0) osm_helpers::write_tmp_relations(w, *formatter, num_relations);
  
  } catch (const std::exception &e) {
    formatter->error(e);
  }

  formatter->end_document();
}
