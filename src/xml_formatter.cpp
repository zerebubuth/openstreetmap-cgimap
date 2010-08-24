#include "xml_formatter.hpp"
#include "temp_tables.hpp"
#include <string>
#include <boost/shared_ptr.hpp>

using std::string;
using boost::shared_ptr;
using std::transform;

xml_formatter::xml_formatter(xml_writer &w, cache<long int, changeset> &cc)
  : writer(w), changeset_cache(cc) {
}

xml_formatter::~xml_formatter() {
}

void
xml_formatter::start_document() {
  writer.start("osm");
  writer.attribute("version", string("0.6"));
  writer.attribute("generator", string(PACKAGE_STRING));
}

void 
xml_formatter::end_document() {
  writer.end();
}

void
xml_formatter::write_bounds(const bbox &bounds) {  
  // bounds element, which seems to be standard in the 
  // main map call.
  writer.start("bounds");
  writer.attribute("minlat", bounds.minlat);
  writer.attribute("minlon", bounds.minlon);
  writer.attribute("maxlat", bounds.maxlat);
  writer.attribute("maxlon", bounds.maxlon);
  writer.end();
}

void 
xml_formatter::start_element_type(element_type type, size_t num_elements) {
  // xml documents surround each element with its type, so there's no
  // need to output any information here.
}

void 
xml_formatter::end_element_type(element_type type) {
  // ditto - nothing needed here for XML.
}

void 
xml_formatter::error(const std::exception &e) {
  // write out an error element to the xml file - we've probably
  // already started writing, so its useless to try and alter the
  // HTTP code.
  writer.start("error");
  writer.text(e.what());
  writer.end();
}

void
xml_formatter::write_tags(pqxx::result &tags) {
  for (pqxx::result::const_iterator itr = tags.begin();
       itr != tags.end(); ++itr) {
    writer.start("tag");
    writer.attribute("k", (*itr)["k"].c_str());
    writer.attribute("v", (*itr)["v"].c_str());
    writer.end();
  }
}

void 
xml_formatter::write_node(const pqxx::result::tuple &r, pqxx::result &tags) {
  const int lat = r["latitude"].as<int>();
  const int lon = r["longitude"].as<int>();
  const long int id = r["id"].as<long int>();
  const long int cs_id = r["changeset_id"].as<long int>();
  shared_ptr<changeset const> cs = changeset_cache.get(cs_id);

  writer.start("node");
  writer.attribute("id", id);
  writer.attribute("lat", double(lat) / double(SCALE));
  writer.attribute("lon", double(lon) / double(SCALE));
  if (cs->data_public) {
    writer.attribute("user", cs->display_name);
    writer.attribute("uid", cs->user_id);
  }
  writer.attribute("visible", r["visible"].as<bool>());
  writer.attribute("version", r["version"].as<int>());
  writer.attribute("changeset", cs_id);
  writer.attribute("timestamp", r["timestamp"].c_str());

  write_tags(tags);

  writer.end();
}

void 
xml_formatter::write_way(const pqxx::result::tuple &r, pqxx::result &nodes, pqxx::result &tags) {
  const long int id = r["id"].as<long int>();
  const long int cs_id = r["changeset_id"].as<long int>();
  shared_ptr<changeset const> cs = changeset_cache.get(cs_id);

  writer.start("way");
  writer.attribute("id", id);
  if (cs->data_public) {
    writer.attribute("user", cs->display_name);
    writer.attribute("uid", cs->user_id);
  }
  writer.attribute("visible", r["visible"].as<bool>());
  writer.attribute("version", r["version"].as<int>());
  writer.attribute("changeset", cs_id);
  writer.attribute("timestamp", r["timestamp"].c_str());

  for (pqxx::result::const_iterator itr = nodes.begin();
       itr != nodes.end(); ++itr) {
    writer.start("nd");
    writer.attribute("ref", (*itr)[0].as<long long int>());
    writer.end();
  }

  write_tags(tags);

  writer.end();
}

void 
xml_formatter::write_relation(const pqxx::result::tuple &r, pqxx::result &members, pqxx::result &tags) {
  const long int id = r["id"].as<long int>();
  const long int cs_id = r["changeset_id"].as<long int>();
  shared_ptr<changeset const> cs = changeset_cache.get(cs_id);

  writer.start("relation");
  writer.attribute("id", id);
  if (cs->data_public) {
    writer.attribute("user", cs->display_name);
    writer.attribute("uid", cs->user_id);
  }
  writer.attribute("visible", r["visible"].as<bool>());
  writer.attribute("version", r["version"].as<int>());
  writer.attribute("changeset", cs_id);
  writer.attribute("timestamp", r["timestamp"].c_str());

  for (pqxx::result::const_iterator itr = members.begin();
       itr != members.end(); ++itr) {
    string type = (*itr)[0].c_str();
    transform(type.begin(), type.end(), type.begin(), ::tolower);
    writer.start("member");
    writer.attribute("type", type);
    writer.attribute("ref", (*itr)[1].as<long long int>());
    writer.attribute("role", (*itr)[2].c_str());
    writer.end();
  }

  write_tags(tags);

  writer.end();
}

