#include "cgimap/text_formatter.hpp"
#include "cgimap/config.hpp"
#include <string>
#include <stdexcept>

using std::string;
using std::shared_ptr;
using std::transform;



text_formatter::text_formatter(text_writer *w) : writer(w) {}

text_formatter::~text_formatter() = default;

mime::type text_formatter::mime_type() const { return mime::text_plain; }

void text_formatter::start_document(
  const std::string &generator, const std::string &root_name) {
  // nothing needed here
}

void text_formatter::end_document() { }

void text_formatter::write_bounds(const bbox &bounds) {
  // nothing needed here
}

void text_formatter::start_element_type(element_type) {
  // nothing needed here
}

void text_formatter::end_element_type(element_type) {
  // nothing needed here
}

void text_formatter::start_action(action_type type) {
  // nothing needed here
}

void text_formatter::end_action(action_type type) {
  // nothing needed here
}

void text_formatter::error(const std::exception &e) {

  writer->text(e.what());
}

void text_formatter::write_tags(const tags_t &tags) {
  // nothing needed here
}

void text_formatter::write_common(const element_info &elem) {
  // nothing needed here
}

void text_formatter::write_node(const element_info &elem, double lon, double lat,
                               const tags_t &tags) {
  // nothing needed here
}

void text_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                              const tags_t &tags) {
  // nothing needed here
}

void text_formatter::write_relation(const element_info &elem,
                                   const members_t &members,
                                   const tags_t &tags) {
  // nothing needed here
}

void text_formatter::write_changeset(const changeset_info &elem,
                                    const tags_t &tags,
                                    bool include_comments,
                                    const comments_t &comments,
                                    const std::chrono::system_clock::time_point &now) {
  // nothing needed here
}

void text_formatter::write_diffresult_create_modify(const element_type elem,
                                            const osm_nwr_signed_id_t old_id,
                                            const osm_nwr_id_t new_id,
                                            const osm_version_t new_version)
{
  // nothing needed here
}


void text_formatter::write_diffresult_delete(const element_type elem,
                                            const osm_nwr_signed_id_t old_id)
{
  // nothing needed here
}



void text_formatter::flush() { writer->flush(); }

void text_formatter::error(const std::string &s) { writer->error(s); }
