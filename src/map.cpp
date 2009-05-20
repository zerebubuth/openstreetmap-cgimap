#include "map.hpp"
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <vector>
#include <string>
#include <pqxx/pqxx>

#include "temp_tables.hpp"
#include "split_tags.hpp"
#include "cache.hpp"

using std::vector;
using std::string;
using boost::shared_ptr;

/**
 * utility class to keep state while writing the document.
 */
struct map_writer {
  xml_writer &writer;
  pqxx::work &w;
  cache<long int, changeset> &changeset_cache;

  map_writer(xml_writer &wr, pqxx::work &wk, cache<long int, changeset> &cc);

  void write();

  void write_node(const pqxx::result::tuple &r);
  void write_way(const pqxx::result::tuple &r);
  void write_relation(const pqxx::result::tuple &r);
};

map_writer::map_writer(xml_writer &wr, pqxx::work &wk, cache<long int, changeset> &cc) 
  : writer(wr), w(wk), changeset_cache(cc) {
}

void 
map_writer::write_node(const pqxx::result::tuple &r) {
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

  pqxx::result tags = w.exec("select k, v from current_node_tags where id=" + pqxx::to_string(id));
  for (pqxx::result::const_iterator itr = tags.begin();
       itr != tags.end(); ++itr) {
    writer.start("tag");
    writer.attribute("k", (*itr)["k"].c_str());
    writer.attribute("v", (*itr)["v"].c_str());
    writer.end();
  }
  writer.end();
}

void
map_writer::write_way(const pqxx::result::tuple &r) {
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

  pqxx::result nodes = w.exec("select node_id from current_way_nodes where id=" + 
			      pqxx::to_string(id) + " order by sequence_id asc");
  for (pqxx::result::const_iterator itr = nodes.begin();
       itr != nodes.end(); ++itr) {
    writer.start("nd");
    writer.attribute("ref", (*itr)[0].as<long long int>());
    writer.end();
  }

  pqxx::result tags = w.exec("select k, v from current_way_tags where id=" + pqxx::to_string(id));
  for (pqxx::result::const_iterator itr = tags.begin();
       itr != tags.end(); ++itr) {
    writer.start("tag");
    writer.attribute("k", (*itr)["k"].c_str());
    writer.attribute("v", (*itr)["v"].c_str());
    writer.end();
  }
  writer.end();
}

void
map_writer::write_relation(const pqxx::result::tuple &r) {
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

  pqxx::result members = w.exec("select member_type, member_id, member_role from "
				"current_relation_members where id=" + 
				pqxx::to_string(id) + " order by sequence_id asc");
  for (pqxx::result::const_iterator itr = members.begin();
       itr != members.end(); ++itr) {
    writer.start("nd");
    writer.attribute("type", (*itr)[0].c_str()); // TODO: downcase
    writer.attribute("ref", (*itr)[1].as<long long int>());
    writer.attribute("role", (*itr)[2].c_str());
    writer.end();
  }

  pqxx::result tags = w.exec("select k, v from current_relation_tags where id=" + pqxx::to_string(id));
  for (pqxx::result::const_iterator itr = tags.begin();
       itr != tags.end(); ++itr) {
    writer.start("tag");
    writer.attribute("k", (*itr)["k"].c_str());
    writer.attribute("v", (*itr)["v"].c_str());
    writer.end();
  }
  writer.end();
}

void
map_writer::write() {
    // get all nodes - they already contain their own tags, so
    // we don't need to do anything else.
    pqxx::result nodes = w.exec(
      "select n.id, n.latitude, n.longitude, n.visible, "
      "to_char(n.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp, "
      "n.changeset_id, n.version from current_nodes n join ("
      "select id from tmp_nodes union distinct select wn.node_id "
      "from tmp_ways w join current_way_nodes wn on w.id = wn.id) x "
      "on n.id = x.id");
    for (pqxx::result::const_iterator itr = nodes.begin(); 
	 itr != nodes.end(); ++itr) {
      write_node(*itr);
    }
    
    // grab the ways, way nodes and tags
    // way nodes and tags are on a separate connections so that the
    // entire result set can be streamed from a single query.
    pqxx::result ways = w.exec(
      "select w.id, w.visible, w.version, w.changeset_id, "
      "to_char(w.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
      "current_ways w join tmp_ways tw on w.id=tw.id where w.visible = true");
    for (pqxx::result::const_iterator itr = ways.begin(); 
	 itr != ways.end(); ++itr) {
      write_way(*itr);
    }
    
    pqxx::result relations = w.exec(
      "select r.id, r.visible, r.version, r.changeset_id, "
      "to_char(r.timestamp,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as timestamp from "
      "current_relations r join (select m.id from "
      "current_relation_members m join tmp_nodes n on m.member_id"
      "=n.id and m.member_type='Node' union distinct select m.id from "
      "current_relation_members m join tmp_ways w on m.member_id"
      "=w.id and m.member_type='Way') x on x.id=r.id where r.visible = true");
    for (pqxx::result::const_iterator itr = relations.begin(); 
	 itr != relations.end(); ++itr) {
      write_relation(*itr);
    }
}

void
write_map(pqxx::work &w,
	  xml_writer &writer,
	  const bbox &bounds,
	  cache<long int, changeset> &changeset_cache) {
  try {
    writer.start("osm");
    writer.attribute("version", string("0.6"));
    writer.attribute("generator", string(PACKAGE_STRING));
    
    // bounds element, which seems to be standard in the 
    // main map call.
    writer.start("bounds");
    writer.attribute("minlat", bounds.minlat);
    writer.attribute("minlon", bounds.minlon);
    writer.attribute("maxlat", bounds.maxlat);
    writer.attribute("maxlon", bounds.maxlon);
    writer.end();

    map_writer mwriter(writer, w, changeset_cache);
    mwriter.write();
  
  } catch (const std::exception &e) {
    // write out an error element to the xml file - we've probably
    // already started writing, so its useless to try and alter the
    // HTTP code.
    writer.start("error");
    writer.text(e.what());
    writer.end();
  }

  writer.end();  
}

