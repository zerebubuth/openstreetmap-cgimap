#!/usr/bin/env ruby

require 'xml/libxml'
require 'set'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
conn.transaction do |tx|
  load_osm_file("#{File.dirname(__FILE__)}/test_anon.osm", tx)
end

# grabs a single node, which has been created by an anonymous user.
# note that external (non-OSM) databases don't have anonymous user
# information exported (because it's anonymous) and so generally
# use a negative number or zero to indicate an anonymous user.
test_request('GET', '/api/0.6/map?bbox=-0.0005,-0.0005,0.0005,0.0005') do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 2, "Number of children of the <osm> element.")

  # first child should be <bounds>
  assert(children[0].name, 'bounds', 'First element in map response should be bounds.')

  # only other element should be the single node.
  node = children[1]
  assert(node.name, "node", "Name of <osm> child.")
  assert(node["id"].to_i, 1, "ID of node")
  assert(node["lat"], "0.0000000", "Latitude attribute")
  assert(node["lon"], "0.0000000", "Longitude attribute")
  assert(node["user"], nil, "User name")
  assert(node["uid"], nil, "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 1, "Version attribute")
  assert(node["changeset"].to_i, 1, "Changeset ID")
  assert(node["timestamp"], "2013-02-16T19:02:00Z", "Timestamp")
end

# grabs two nodes, with the anonymous one coming second. the anonymous
# one should not copy the user information from the first one.
test_request('GET', '/api/0.6/nodes?nodes=2,3') do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 2, "Number of children of the <osm> element.")

  normal_node = children[0]
  assert(normal_node.name, 'node', 'First child should be a node')
  assert(normal_node['id'].to_i, 2, 'ID of node')
  assert(normal_node['uid'], "2", 'User ID of normal node')
  assert(normal_node['user'], "foo", 'User name of normal node')

  anon_node = children[1]
  assert(anon_node.name, 'node', 'First child should be a node')
  assert(anon_node['id'].to_i, 3, 'ID of node')
  assert(anon_node['uid'], nil, 'User ID of anon node')
  assert(anon_node['user'], nil, 'User name of anon node')
end

# same test as above, but for a map request
test_request('GET', '/api/0.6/map?bbox=0.9995,0.9995,1.0005,1.0005') do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 3, "Number of children of the <osm> element.")

  # first child should be <bounds>
  assert(children[0].name, 'bounds', 'First element in map response should be bounds.')

  normal_node = children[1]
  assert(normal_node.name, 'node', 'First child should be a node')
  assert(normal_node['id'].to_i, 2, 'ID of node')
  assert(normal_node['uid'], "2", 'User ID of normal node')
  assert(normal_node['user'], "foo", 'User name of normal node')

  anon_node = children[2]
  assert(anon_node.name, 'node', 'First child should be a node')
  assert(anon_node['id'].to_i, 3, 'ID of node')
  assert(anon_node['uid'], nil, 'User ID of anon node')
  assert(anon_node['user'], nil, 'User name of anon node')
end
