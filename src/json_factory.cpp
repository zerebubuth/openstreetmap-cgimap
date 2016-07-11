#include "cgimap/json_factory.hpp"

json_factory::json_factory() {}

json_factory::~json_factory() {}

json_writer &json_factory::create_writer(boost::shared_ptr<output_buffer> &out,
                                         bool indent) {
  writer = new json_writer(out, indent);
  return *writer;
}

json_formatter &
json_factory::create_formatter(cache<osm_changeset_id_t, changeset> &changeset_cache) {
  formatter = new json_formatter(*writer, changeset_cache);
  return *formatter;
}
