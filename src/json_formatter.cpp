
#include "cgimap/json_formatter.hpp"
#include "cgimap/config.hpp"

#include <chrono>

using std::shared_ptr;
using std::string;
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
    throw std::runtime_error("Unknown element type in element_type_name().");
  }
}

} // anonymous namespace

json_formatter::json_formatter(json_writer *w) : writer(w),
    is_in_elements_array(false) {}

json_formatter::~json_formatter() = default;

mime::type json_formatter::mime_type() const { return mime::text_json; }

void json_formatter::write_tags(const tags_t &tags) {

  if (tags.empty())
    return;

  writer->object_key("tags");
  writer->start_object();
  for (const auto& tag : tags) {
    writer->object_key(tag.first);
    writer->entry_string(tag.second);
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

void json_formatter::end_document() {

  writer->end_array();            // end of elements array
  is_in_elements_array = false;
  writer->end_object();
}

void json_formatter::start_element_type(element_type type) {

  if (is_in_elements_array)
    return;

  writer->object_key("elements");
  writer->start_array();
  is_in_elements_array = true;
}

void json_formatter::end_element_type(element_type type) {}

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

void json_formatter::write_id(const element_info &elem) {
  writer->object_key("id");
  writer->entry_int(elem.id);
}


void json_formatter::write_common(const element_info &elem) {
  writer->object_key("timestamp");
  writer->entry_string(elem.timestamp);
  writer->object_key("version");
  writer->entry_int(elem.version);
  writer->object_key("changeset");
  writer->entry_int(elem.changeset);
  if (elem.display_name && elem.uid) {
    writer->object_key("user");
    writer->entry_string(elem.display_name.get());
    writer->object_key("uid");
    writer->entry_int(elem.uid.get());
  }
  // At this itme, only the map call is really supported for JSON output,
  // where all elements are expected to be visible.
  if (!elem.visible) {
    writer->object_key("visible");
    writer->entry_bool(elem.visible);
  }
}

void json_formatter::write_node(const element_info &elem, double lon,
                                double lat, const tags_t &tags) {
  writer->start_object();

  WRITE_KV("type", string, "node");

  write_id(elem);
  if (elem.visible) {
    writer->object_key("lat");
    writer->entry_double(lat);
    writer->object_key("lon");
    writer->entry_double(lon);
  }
  write_common(elem);
  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_way(const element_info &elem, const nodes_t &nodes,
                               const tags_t &tags) {
  writer->start_object();

  WRITE_KV("type", string, "way");

  write_id(elem);
  write_common(elem);
  writer->object_key("nodes");
  writer->start_array();
  for (const auto &node : nodes) {
    writer->entry_int(node);
  }
  writer->end_array();

  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_relation(const element_info &elem,
                                    const members_t &members,
                                    const tags_t &tags) {
  writer->start_object();

  WRITE_KV("type", string, "relation");

  write_id(elem);
  write_common(elem);

  if (!members.empty()) {
      writer->object_key("members");
      writer->start_array();
      for (const auto & member : members) {
	  writer->start_object();
	  writer->object_key("type");
	  writer->entry_string(element_type_name(member.type));
	  writer->object_key("ref");
	  writer->entry_int(member.ref);
	  writer->object_key("role");
	  writer->entry_string(member.role);
	  writer->end_object();
      }
      writer->end_array();
  }

  write_tags(tags);

  writer->end_object();
}

void json_formatter::write_changeset(const changeset_info &elem,
                                     const tags_t &tags,
                                     bool include_comments,
                                     const comments_t &comments,
                                     const std::chrono::system_clock::time_point &now) {

  writer->start_object();

  WRITE_KV("type", string, "changeset");

  writer->object_key("id");
  writer->entry_int(elem.id);

  writer->object_key("created_at");
  writer->entry_string(elem.created_at);

  const bool is_open = elem.is_open_at(now);
  if (!is_open) {
      writer->object_key("closed_at");
      writer->entry_string(elem.closed_at);
  }

  writer->object_key("open");
  writer->entry_bool(is_open);

  if (elem.display_name && bool(elem.uid)) {
    writer->object_key("user");
    writer->entry_string(elem.display_name.get());
    writer->object_key("uid");
    writer->entry_int(elem.uid.get());
  }

  if (elem.bounding_box) {
      writer->object_key("minlat");
      writer->entry_double(elem.bounding_box->minlat);
      writer->object_key("minlon");
      writer->entry_double(elem.bounding_box->minlon);
      writer->object_key("maxlat");
      writer->entry_double(elem.bounding_box->maxlat);
      writer->object_key("maxlon");
      writer->entry_double(elem.bounding_box->maxlon);
  }

  writer->object_key("comments_count");
  writer->entry_int(elem.comments_count);

  writer->object_key("changes_count");
  writer->entry_int(elem.num_changes);

  write_tags(tags);

  if (include_comments && !comments.empty()) {
      writer->object_key("discussion");
      writer->start_array();
      for (const auto & comment : comments) {
	  writer->start_object();
	  writer->object_key("date");
	  writer->entry_string(comment.created_at);
	  writer->object_key("uid");
	  writer->entry_int(comment.author_id);
	  writer->object_key("user");
	  writer->entry_string(comment.author_display_name);
	  writer->object_key("text");
	  writer->entry_string(comment.body);
	  writer->end_object();
      }
      writer->end_array();
  }

  writer->end_object();
}

void json_formatter::write_diffresult_create_modify(const element_type elem,
                                            const osm_nwr_signed_id_t old_id,
                                            const osm_nwr_id_t new_id,
                                            const osm_version_t new_version)
{

//  writer->start_object();
//  writer->object_key("type");
//  writer->entry_string(element_type_name(elem));
//  writer->object_key("old_id");
//  writer->entry_int(old_id);
//  writer->object_key("new_id");
//  writer->entry_int(new_id);
//  writer->object_key("new_version");
//  writer->entry_int(new_version);
//  writer->end_object();
}


void json_formatter::write_diffresult_delete(const element_type elem,
                                            const osm_nwr_signed_id_t old_id)
{
//  writer->start_object();
//  writer->object_key("type");
//  writer->entry_string(element_type_name(elem));
//  writer->object_key("old_id");
//  writer->entry_int(old_id);
//  writer->end_object();
}

void json_formatter::flush() { writer->flush(); }

void json_formatter::error(const std::string &s) { writer->error(s); }
