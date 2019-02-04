#include "cgimap/xml_formatter.hpp"
#include "cgimap/config.hpp"
#include <string>
#include <stdexcept>

using std::string;
using std::shared_ptr;
using std::transform;

namespace {

const std::string &element_type_name(element_type elt) {
  static std::string name_node("node"), name_way("way"),
      name_relation("relation");

  switch (elt) {
  case element_type_node:
    return name_node;
  case element_type_way:
    return name_way;
  case element_type_relation:
    return name_relation;
  default:
    // in case the switch isn't exhaustive?
    throw std::runtime_error("Unhandled element type in element_type_name().");
  }
}

} // anonymous namespace

xml_formatter::xml_formatter(xml_writer *w) : writer(w) {}

xml_formatter::~xml_formatter() = default;

mime::type xml_formatter::mime_type() const { return mime::text_xml; }

void xml_formatter::start_document(
  const std::string &generator, const std::string &root_name) {
  writer->start(root_name);
  writer->attribute("version", string("0.6"));
  writer->attribute("generator", generator);

  writer->attribute("copyright", string("OpenStreetMap and contributors"));
  writer->attribute("attribution",
                    string("http://www.openstreetmap.org/copyright"));
  writer->attribute("license",
                    string("http://opendatacommons.org/licenses/odbl/1-0/"));
}

void xml_formatter::end_document() { writer->end(); }

void xml_formatter::write_bounds(const bbox &bounds) {
  // bounds element, which seems to be standard in the
  // main map call.
  writer->start("bounds");
  writer->attribute("minlat", bounds.minlat);
  writer->attribute("minlon", bounds.minlon);
  writer->attribute("maxlat", bounds.maxlat);
  writer->attribute("maxlon", bounds.maxlon);
  writer->end();
}

void xml_formatter::start_element_type(element_type) {
  // xml documents surround each element with its type, so there's no
  // need to output any information here.
}

void xml_formatter::end_element_type(element_type) {
  // ditto - nothing needed here for XML.
}

void xml_formatter::start_action(action_type type) {
  switch (type) {
  case action_type_create:
    writer->start("create");
    break;
  case action_type_modify:
    writer->start("modify");
    break;
  case action_type_delete:
    writer->start("delete");
    break;
  }
}

void xml_formatter::end_action(action_type type) {
  switch (type) {
  case action_type_create:
  case action_type_modify:
  case action_type_delete:
    writer->end();
    break;
  }
}

void xml_formatter::error(const std::exception &e) {
  // write out an error element to the xml file - we've probably
  // already started writing, so its useless to try and alter the
  // HTTP code.
  writer->start("error");
  writer->text(e.what());
  writer->end();
}

void xml_formatter::write_tags(const tags_t &tags) {
  for (const auto & tag : tags) {
    writer->start("tag");
    writer->attribute("k", tag.first);
    writer->attribute("v", tag.second);
    writer->end();
  }
}

void xml_formatter::write_common(const element_info &elem) {
  writer->attribute("id", elem.id);
  writer->attribute("visible", elem.visible);
  writer->attribute("version", elem.version);
  writer->attribute("changeset", elem.changeset);
  writer->attribute("timestamp", elem.timestamp);
  if (elem.display_name && elem.uid) {
    writer->attribute("user", elem.display_name.get());
    writer->attribute("uid", elem.uid.get());
  }
}

void xml_formatter::write_node(const element_info &elem, double lon, double lat,
                               const tags_t &tags) {
  writer->start("node");
  write_common(elem);
  if (elem.visible) {
    writer->attribute("lat", lat);
    writer->attribute("lon", lon);
  }
  write_tags(tags);

  writer->end();
}

void xml_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                              const tags_t &tags) {
  writer->start("way");
  write_common(elem);

  for (osm_nwr_id_t node : nodes) {
    writer->start("nd");
    writer->attribute("ref", node);
    writer->end();
  }

  write_tags(tags);

  writer->end();
}

void xml_formatter::write_relation(const element_info &elem,
                                   const members_t &members,
                                   const tags_t &tags) {
  writer->start("relation");
  write_common(elem);

  for (const auto & member : members) {
    writer->start("member");
    writer->attribute("type", element_type_name(member.type));
    writer->attribute("ref", member.ref);
    writer->attribute("role", member.role);
    writer->end();
  }

  write_tags(tags);

  writer->end();
}

void xml_formatter::write_changeset(const changeset_info &elem,
                                    const tags_t &tags,
                                    bool include_comments,
                                    const comments_t &comments,
                                    const std::chrono::system_clock::time_point &now) {
  writer->start("changeset");

  writer->attribute("id", elem.id);
  writer->attribute("created_at", elem.created_at);
  const bool is_open = elem.is_open_at(now);
  if (!is_open) {
    writer->attribute("closed_at", elem.closed_at);
  }
  writer->attribute("open", is_open);

  if (bool(elem.display_name) && bool(elem.uid)) {
    writer->attribute("user", elem.display_name.get());
    writer->attribute("uid", elem.uid.get());
  }

  if (elem.bounding_box) {
    writer->attribute("min_lat", elem.bounding_box->minlat);
    writer->attribute("min_lon", elem.bounding_box->minlon);
    writer->attribute("max_lat", elem.bounding_box->maxlat);
    writer->attribute("max_lon", elem.bounding_box->maxlon);
  }

  writer->attribute("comments_count", elem.comments_count);
  writer->attribute("changes_count", elem.num_changes);

  write_tags(tags);

  if (include_comments) {
    writer->start("discussion");
    for (const auto & comment : comments) {
      writer->start("comment");
      writer->attribute("date", comment.created_at);
      writer->attribute("uid", comment.author_id);
      writer->attribute("user", comment.author_display_name);
      writer->start("text");
      writer->text(comment.body);
      writer->end();
      writer->end();
    }
    writer->end();
  }

  writer->end();
}

void xml_formatter::write_diffresult_create_modify(const element_type elem,
                                            const osm_nwr_signed_id_t old_id,
                                            const osm_nwr_id_t new_id,
                                            const osm_version_t new_version)
{
  writer->start(element_type_name(elem));
  writer->attribute("old_id", old_id);
  writer->attribute("new_id", new_id);
  writer->attribute("new_version", new_version);
  writer->end();
}


void xml_formatter::write_diffresult_delete(const element_type elem,
                                            const osm_nwr_signed_id_t old_id)
{
  writer->start(element_type_name(elem));
  writer->attribute("old_id", old_id);
  writer->end();
}



void xml_formatter::flush() { writer->flush(); }

void xml_formatter::error(const std::string &s) { writer->error(s); }
