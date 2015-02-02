#include "cgimap/xml_formatter.hpp"
#include "cgimap/config.hpp"
#include <string>
#include <boost/shared_ptr.hpp>
#include <stdexcept>

using std::string;
using boost::shared_ptr;
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

xml_formatter::~xml_formatter() {}

mime::type xml_formatter::mime_type() const { return mime::text_xml; }

void xml_formatter::start_document(const std::string &generator) {
  writer->start("osm");
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

void xml_formatter::error(const std::exception &e) {
  // write out an error element to the xml file - we've probably
  // already started writing, so its useless to try and alter the
  // HTTP code.
  writer->start("error");
  writer->text(e.what());
  writer->end();
}

void xml_formatter::write_tags(const tags_t &tags) {
  for (tags_t::const_iterator itr = tags.begin(); itr != tags.end(); ++itr) {
    writer->start("tag");
    writer->attribute("k", itr->first);
    writer->attribute("v", itr->second);
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

  for (nodes_t::const_iterator itr = nodes.begin(); itr != nodes.end(); ++itr) {
    writer->start("nd");
    writer->attribute("ref", *itr);
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

  for (members_t::const_iterator itr = members.begin(); itr != members.end();
       ++itr) {
    writer->start("member");
    writer->attribute("type", element_type_name(itr->type));
    writer->attribute("ref", itr->ref);
    writer->attribute("role", itr->role);
    writer->end();
  }

  write_tags(tags);

  writer->end();
}

void xml_formatter::flush() { writer->flush(); }

void xml_formatter::error(const std::string &s) { writer->error(s); }
