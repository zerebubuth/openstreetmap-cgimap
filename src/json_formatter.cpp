#include <boost/shared_ptr.hpp>

#include "cgimap/json_formatter.hpp"
#include "cgimap/config.hpp"

using boost::shared_ptr;
using std::string;
using std::transform;
namespace pt = boost::posix_time;

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
    throw std::runtime_error("Unknown element type in element_type_name().");
  }
}

} // anonymous namespace

json_formatter::json_formatter(json_writer *w) : writer(w) {}

json_formatter::~json_formatter() {}

mime::type json_formatter::mime_type() const { return mime::text_json; }

void json_formatter::write_tags(const tags_t &tags) {
  writer->object_key("tags");
  writer->start_object();
  for (tags_t::const_iterator itr = tags.begin(); itr != tags.end(); ++itr) {
    writer->object_key(itr->first);
    writer->entry_string(itr->second);
  }
  writer->end_object();
}

#define WRITE_KV(k,vt,v) \
  writer->object_key(k); \
  writer->entry_##vt(v);

void json_formatter::start_document(
  const std::string &generator, const std::string &root_name) {
  writer->start_object();

  WRITE_KV("version", string, "0.6");
  WRITE_KV("generator", string, generator);
  WRITE_KV("copyright", string, "OpenStreetMap and contributors");
  WRITE_KV("attribution", string, "http://www.openstreetmap.org/copyright");
  WRITE_KV("license", string, "http://opendatacommons.org/licenses/odbl/1-0/");
}

void json_formatter::write_bounds(const bbox &bounds) {
  writer->object_key("bounds");
  writer->start_object();
  writer->object_key("minlat");
  writer->entry_double(bounds.minlat);
  writer->object_key("minlon");
  writer->entry_double(bounds.minlon);
  writer->object_key("maxlat");
  writer->entry_double(bounds.maxlat);
  writer->object_key("maxlon");
  writer->entry_double(bounds.maxlon);
  writer->end_object();
}

void json_formatter::end_document() { writer->end_object(); }

void json_formatter::start_element_type(element_type type) {
  if (type == element_type_changeset) {
    writer->object_key("changesets");
  } else if (type == element_type_node) {
    writer->object_key("nodes");
  } else if (type == element_type_way) {
    writer->object_key("ways");
  } else {
    writer->object_key("relations");
  }
  writer->start_array();
}

void json_formatter::end_element_type(element_type) { writer->end_array(); }

void json_formatter::start_action(action_type type) {
}

void json_formatter::end_action(action_type type) {
}

void json_formatter::error(const std::exception &e) {
  writer->start_object();
  writer->object_key("error");
  writer->entry_string(e.what());
  writer->end_object();
}

void json_formatter::write_common(const element_info &elem) {
  writer->object_key("id");
  writer->entry_int(elem.id);
  writer->object_key("visible");
  writer->entry_bool(elem.visible);
  writer->object_key("version");
  writer->entry_int(elem.version);
  writer->object_key("changeset");
  writer->entry_int(elem.changeset);
  writer->object_key("timestamp");
  writer->entry_string(elem.timestamp);
  if (elem.display_name && elem.uid) {
    writer->object_key("user");
    writer->entry_string(elem.display_name.get());
    writer->object_key("uid");
    writer->entry_int(elem.uid.get());
  }
}

void json_formatter::write_node(const element_info &elem, double lon,
                                double lat, const tags_t &tags) {
  writer->start_object();

  write_common(elem);
  if (elem.visible) {
    writer->object_key("lat");
    writer->entry_double(lat);
    writer->object_key("lon");
    writer->entry_double(lon);
  }
  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                               const tags_t &tags) {
  writer->start_object();

  write_common(elem);
  writer->object_key("nds");
  writer->start_array();
  for (nodes_t::const_iterator itr = nodes.begin(); itr != nodes.end(); ++itr) {
    writer->entry_int(*itr);
  }
  writer->end_array();

  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_relation(const element_info &elem,
                                    const members_t &members,
                                    const tags_t &tags) {
  writer->start_object();

  write_common(elem);
  writer->object_key("members");
  writer->start_array();
  for (members_t::const_iterator itr = members.begin(); itr != members.end();
       ++itr) {
    writer->start_object();
    writer->object_key("type");
    writer->entry_string(element_type_name(itr->type));
    writer->object_key("ref");
    writer->entry_int(itr->ref);
    writer->object_key("role");
    writer->entry_string(itr->role);
    writer->end_object();
  }
  writer->end_array();

  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_changeset(const changeset_info &elem,
                                     const tags_t &tags,
                                     bool include_comments,
                                     const comments_t &comments,
                                     const pt::ptime &now) {
  writer->start_object();
  write_tags(tags);
  writer->end_object();
}

void json_formatter::flush() { writer->flush(); }

void json_formatter::error(const std::string &s) { writer->error(s); }
