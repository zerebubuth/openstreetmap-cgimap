#include <boost/shared_ptr.hpp>

#include "json_formatter.hpp"
#include "temp_tables.hpp"

using boost::shared_ptr;
using std::string;
using std::transform;

json_formatter::json_formatter(json_writer &w, cache<long int, changeset> &cc) 
  : writer(w), changeset_cache(cc) {
}

json_formatter::~json_formatter() {
}

void 
json_formatter::write_tags(pqxx::result &tags) {
  if (tags.size() > 0) {
    writer.object_key("tags");
    writer.start_object();
    for (pqxx::result::const_iterator itr = tags.begin();
	 itr != tags.end(); ++itr) {
      writer.object_key((*itr)["k"].c_str()); 
      writer.entry_string((*itr)["v"].c_str());
    }
    writer.end_object();
  }
}

void 
json_formatter::start_document() {
  writer.start_object();
}

void
json_formatter::write_bounds(const bbox &bounds) {
  writer.object_key("bounds");
  writer.start_object();
  writer.object_key("minlat"); writer.entry_double(bounds.minlat);
  writer.object_key("minlon"); writer.entry_double(bounds.minlon);
  writer.object_key("maxlat"); writer.entry_double(bounds.maxlat);
  writer.object_key("maxlon"); writer.entry_double(bounds.maxlon);
  writer.end_object();
}

void 
json_formatter::end_document() {
  writer.end_object();
}

void 
json_formatter::start_element_type(element_type type, size_t num_elements) {
  if (type == element_type_node) {
    writer.object_key("nodes");
  } else if (type == element_type_way) {
    writer.object_key("ways");
  } else {
    writer.object_key("relations");
  }
  writer.start_array();
}

void 
json_formatter::end_element_type(element_type type) {
  writer.end_array();
}

void 
json_formatter::error(const std::exception &e) {
  writer.start_object();
  writer.object_key("error");
  writer.entry_string(e.what());
  writer.end_object();
}

void 
json_formatter::write_node(const pqxx::result::tuple &r, pqxx::result &tags) {
  const int lat = r["latitude"].as<int>();
  const int lon = r["longitude"].as<int>();
  const long int id = r["id"].as<long int>();
  const long int cs_id = r["changeset_id"].as<long int>();
  shared_ptr<changeset const> cs = changeset_cache.get(cs_id);

  writer.start_object();
  writer.object_key("id"); writer.entry_int(id);
  writer.object_key("lat"); writer.entry_double(double(lat) / double(SCALE));
  writer.object_key("lon"); writer.entry_double(double(lon) / double(SCALE));
  if (cs->data_public) {
    writer.object_key("user"); writer.entry_string(cs->display_name);
    writer.object_key("uid"); writer.entry_int(cs->user_id);
  }
  writer.object_key("visible"); writer.entry_bool(r["visible"].as<bool>());
  writer.object_key("version"); writer.entry_int(r["version"].as<int>());
  writer.object_key("changeset"); writer.entry_int(cs_id);
  writer.object_key("timestamp"); writer.entry_string(r["timestamp"].c_str());

  write_tags(tags);

  writer.end_object();  
}

void 
json_formatter::write_way(const pqxx::result::tuple &r, pqxx::result &nodes, pqxx::result &tags) {
  const long int id = r["id"].as<long int>();
  const long int cs_id = r["changeset_id"].as<long int>();
  shared_ptr<changeset const> cs = changeset_cache.get(cs_id);

  writer.start_object();
  writer.object_key("id"); writer.entry_int(id);
  if (cs->data_public) {
    writer.object_key("user"); writer.entry_string(cs->display_name);
    writer.object_key("uid"); writer.entry_int(cs->user_id);
  }
  writer.object_key("visible"); writer.entry_bool(r["visible"].as<bool>());
  writer.object_key("version"); writer.entry_int(r["version"].as<int>());
  writer.object_key("changeset"); writer.entry_int(cs_id);
  writer.object_key("timestamp"); writer.entry_string(r["timestamp"].c_str());

  writer.object_key("nds");
  writer.start_array();
  for (pqxx::result::const_iterator itr = nodes.begin();
       itr != nodes.end(); ++itr) {
    writer.entry_int((*itr)[0].as<long long int>());
  }
  writer.end_array();

  write_tags(tags);

  writer.end_object();
}

void 
json_formatter::write_relation(const pqxx::result::tuple &r, pqxx::result &members, pqxx::result &tags) {
  const long int id = r["id"].as<long int>();
  const long int cs_id = r["changeset_id"].as<long int>();
  shared_ptr<changeset const> cs = changeset_cache.get(cs_id);

  writer.start_object();
  writer.object_key("id"); writer.entry_int(id);
  if (cs->data_public) {
    writer.object_key("user"); writer.entry_string(cs->display_name);
    writer.object_key("uid"); writer.entry_int(cs->user_id);
  }
  writer.object_key("visible"); writer.entry_bool(r["visible"].as<bool>());
  writer.object_key("version"); writer.entry_int(r["version"].as<int>());
  writer.object_key("changeset"); writer.entry_int(cs_id);
  writer.object_key("timestamp"); writer.entry_string(r["timestamp"].c_str());

  writer.object_key("members"); 
  writer.start_array();
  for (pqxx::result::const_iterator itr = members.begin();
       itr != members.end(); ++itr) {
    string type = (*itr)[0].c_str();
    transform(type.begin(), type.end(), type.begin(), ::tolower);
    writer.start_object();
    writer.object_key("type"); writer.entry_string(type);
    writer.object_key("ref"); writer.entry_int((*itr)[1].as<long long int>());
    writer.object_key("role"); writer.entry_string((*itr)[2].c_str());
    writer.end_object();
  }
  writer.end_array();

  write_tags(tags);

  writer.end_object();
}
