#include "cgimap/config.hpp"
#include "cgimap/osm_diffresult_responder.hpp"

using std::list;
using boost::shared_ptr;
namespace pt = boost::posix_time;

osm_diffresult_responder::osm_diffresult_responder(mime::type mt)
    : osm_responder(mt) {}

osm_diffresult_responder::~osm_diffresult_responder() {}

void osm_diffresult_responder::write(shared_ptr<output_formatter> formatter,
                                  const std::string &generator,
                                  const pt::ptime &now) {

  // TODO: is it possible that formatter can be null?
  output_formatter &fmt = *formatter;

  try {
    fmt.start_document(generator, "diffResult");

    // Nodes

    for (const auto& id : change_tracking->created_node_ids)
      fmt.write_diffresult_create_modify(element_type_node, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->modified_node_ids)
      fmt.write_diffresult_create_modify(element_type_node, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->skip_deleted_node_ids)
      fmt.write_diffresult_create_modify(element_type_node, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->deleted_node_ids)
      fmt.write_diffresult_delete(element_type_node, id);


    // Ways

    for (const auto& id : change_tracking->created_way_ids)
      fmt.write_diffresult_create_modify(element_type_way, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->modified_way_ids)
      fmt.write_diffresult_create_modify(element_type_way, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->skip_deleted_way_ids)
      fmt.write_diffresult_create_modify(element_type_way, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->deleted_way_ids)
      fmt.write_diffresult_delete(element_type_way, id);


    // Relations

    for (const auto& id : change_tracking->created_relation_ids)
      fmt.write_diffresult_create_modify(element_type_relation, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->modified_relation_ids)
      fmt.write_diffresult_create_modify(element_type_relation, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->skip_deleted_relation_ids)
      fmt.write_diffresult_create_modify(element_type_relation, id.old_id, id.new_id, id.new_version);

    for (const auto& id : change_tracking->deleted_relation_ids)
      fmt.write_diffresult_delete(element_type_relation, id);


  } catch (const std::exception &e) {
    fmt.error(e);
  }

  fmt.end_document();

}
