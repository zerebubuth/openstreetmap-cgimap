#include "map.hpp"
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <vector>
#include <string>

#include "temp_tables.hpp"
#include "split_tags.hpp"

using std::vector;
using std::string;

void 
write_node(xml_writer &writer, const mysqlpp::Row &r) {
  const int lat = r["latitude"];
  const int lon = r["longitude"];

  writer.start("node");
  writer.attribute("id", r["id"]);
  writer.attribute("lat", double(lat) / double(SCALE));
  writer.attribute("lon", double(lon) / double(SCALE));
  if (r["data_public"]) {
    writer.attribute("user", r["display_name"]);
  }
  writer.attribute("visible", bool(r["visible"]));
  writer.attribute("timestamp", r["timestamp"]);
  const vector<string> tags = tags_split(string(r["tags"]));
  for (vector<string>::const_iterator itr = tags.begin();
       itr != tags.end();) {
    writer.start("tag");
    writer.attribute("k", *itr++);
    writer.attribute("v", *itr++);
    writer.end();
  }
  writer.end();
}

void
write_way(xml_writer &writer, 
	  mysqlpp::Connection &con,
	  const mysqlpp::Row &r) {
  long unsigned int way_id = r["id"];

  writer.start("way");
  writer.attribute("id", r["id"]);
  if (r["data_public"]) {
    writer.attribute("user", r["display_name"]);
  }
  writer.attribute("visible", bool(r["visible"]));
  writer.attribute("timestamp", r["timestamp"]);
  mysqlpp::Query way_nodes = con.query();
  way_nodes << "select node_id from `current_way_nodes` where id=" 
	    << way_id << " order by sequence_id asc";
  if (mysqlpp::UseQueryResult res = way_nodes.use()) {
    while (mysqlpp::Row row = res.fetch_row()) {
      writer.start("nd");
      writer.attribute("ref", (long long int)row[0]);
      writer.end();
    }
  }
  mysqlpp::Query way_tags = con.query();
  way_tags << "select k, v from `current_way_tags` where id=" 
	   << way_id;
  if (mysqlpp::UseQueryResult res = way_tags.use()) {
    while (mysqlpp::Row row = res.fetch_row()) {
      writer.start("tag");
      writer.attribute("k", row["k"]);
      writer.attribute("v", row["v"]);
      writer.end();
    }
  }
  writer.end();
}

void
write_relation(xml_writer &writer, 
	       mysqlpp::Connection &con,
	       const mysqlpp::Row &r) {
  long unsigned int relation_id = r["id"];

  writer.start("relation");
  writer.attribute("id", r["id"]);
  if (r["data_public"]) {
    writer.attribute("user", r["display_name"]);
  }
  writer.attribute("visible", bool(r["visible"]));
  writer.attribute("timestamp", r["timestamp"]);
  mysqlpp::Query relation_members = con.query();
  relation_members << 
    "select member_type, member_id, member_role from "
    "`current_relation_members` where id=" << relation_id;
  if (mysqlpp::UseQueryResult res = relation_members.use()) {
    while (mysqlpp::Row row = res.fetch_row()) {
      writer.start("member");
      writer.attribute("type", row[0]);
      writer.attribute("ref", (long long int)row[1]);
      writer.attribute("role", row[2]);
      writer.end();
    }
  }
  mysqlpp::Query relation_tags = con.query();
  relation_tags << "select k, v from `current_relation_tags` where id=" 
		<< relation_id;
  if (mysqlpp::UseQueryResult res = relation_tags.use()) {
    while (mysqlpp::Row row = res.fetch_row()) {
      writer.start("tag");
      writer.attribute("k", row["k"]);
      writer.attribute("v", row["v"]);
      writer.end();
    }
  }
  writer.end();
}

void
write_map(mysqlpp::Connection &con,
	  mysqlpp::Connection &con2,
	  xml_writer &writer,
	  const bbox &bounds) {
  try {
    writer.start("osm");
    writer.attribute("version", string("0.5"));
    writer.attribute("generator", string("cgimap-0.1"));
    
    // bounds element, which seems to be standard in the 
    // main map call.
    writer.start("bounds");
    writer.attribute("minlat", bounds.minlat);
    writer.attribute("minlon", bounds.minlon);
    writer.attribute("maxlat", bounds.maxlat);
    writer.attribute("maxlon", bounds.maxlon);
    writer.end();

    // get all nodes - they already contain their own tags, so
    // we don't need to do anything else.
    mysqlpp::Query nodes = con.query();
    nodes << 
      "select n.id, n.latitude, n.longitude, u.display_name, u.data_public, "
      "n.visible, n.tags, date_format(n.timestamp,'%Y-%m-%dT%TZ') "
      "as timestamp from `current_nodes` n join ("
      "select id from `tmp_nodes` union distinct select wn.node_id "
      "from `tmp_ways` w join `current_way_nodes` wn on w.id = wn.id) x "
      "on n.id = x.id join `users` u on n.user_id";
    nodes.for_each(boost::bind(write_node, boost::ref(writer), _1));
    
    // grab the ways, way nodes and tags
    // way nodes and tags are on a separate connections so that the
    // entire result set can be streamed from a single query.
    mysqlpp::Query ways = con.query();
    ways <<
      "select w.id, u.display_name, u.data_public, w.visible, "
      "date_format(w.timestamp,'%Y-%m-%dT%TZ') as timestamp from "
      "`current_ways` w join `tmp_ways` tw on w.id=tw.id join "
      "`users` u on w.user_id = u.id";
    ways.for_each(boost::bind(write_way, boost::ref(writer), 
			      boost::ref(con2), _1));
    
    mysqlpp::Query relations = con.query();
    relations << 
      "select r.id, u.display_name, u.data_public, r.visible, "
      "date_format(r.timestamp,'%Y-%m-%dT%TZ') as timestamp from "
      "`current_relations` r join (select m.id from "
      "`current_relation_members` m join `tmp_nodes` n on m.member_id"
      "=n.id and m.member_type='node' union distinct select m.id from "
      "`current_relation_members` m join `tmp_ways` w on m.member_id"
      "=w.id and m.member_type='way') x on x.id=r.id join `users` u "
      "on r.user_id=u.id";
    relations.for_each(boost::bind(write_relation, boost::ref(writer),
				   boost::ref(con2), _1));
  
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
