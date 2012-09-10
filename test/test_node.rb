#!/usr/bin/env ruby

require 'xml/libxml'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
conn.exec("insert into users (id, email, pass_crypt, creation_time, display_name, data_public) values (1, 'foo@example.com', '', now(), 'foo', true)")
conn.exec("insert into changesets (id, user_id, created_at, min_lat, max_lat, min_lon, max_lon, closed_at, num_changes) values (1, 1, now(), 0, 0, 0, 0, now(), 1)")
conn.exec("insert into current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) values (1, 0, 0, 1, true, now(), 0, 1)")

test_request("GET", "/api/0.6/node/1", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 1, "Number of children of the <osm> element.")
  node = children[0]
  assert(node.name, "node", "Name of <osm> child.")
  assert(node["id"].to_i, 1, "ID of node")
  assert(node["lat"], "0.0000000", "Latitude attribute")
  assert(node["lon"], "0.0000000", "Longitude attribute")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], "1", "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 1, "Version attribute")
  assert(node["changeset"].to_i, 1, "Changeset ID")
  # TODO: figure out how to test this...
  # timestamp="2012-09-10T08:06:58Z"
end

