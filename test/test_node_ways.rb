#!/usr/bin/env ruby

require 'xml/libxml'
require 'set'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
load_osm_file("#{File.dirname(__FILE__)}/test_node_ways.osm", conn)

test_request("GET", "/api/0.6/node/1/ways", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")
  assert(headers.has_key?("Content-Disposition"), false, "Response content disposition header.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 2, "Number of children of the <osm> element.")
  node = children[0]
  assert(node.name, "node", "Name of first <osm> child.")
  assert(node["id"].to_i, 1, "ID of node")
  assert(node["lat"], "0.0000000", "Latitude attribute")
  assert(node["lon"], "0.0000000", "Longitude attribute")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], "1", "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 1, "Version attribute")
  assert(node["changeset"].to_i, 1, "Changeset ID")
  assert(node["timestamp"], "2012-09-25T00:00:00Z", "Timestamp")

  node = children[1]

  assert(node.name, "way", "Name of second <osm> child.")
  assert(node["id"].to_i, 1, "ID of way")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], "1", "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 1, "Version attribute")
  assert(node["changeset"].to_i, 1, "Changeset ID")
  assert(node["timestamp"], "2012-12-01T00:00:00Z", "Timestamp")

  nds = node.children.select {|n| n.element? && n.name == "nd" }
  assert(nds.size, 4, "Number of way nodes.")
  assert(nds[0]['ref'].to_i, 1, "First way node ref.")
  assert(nds[1]['ref'].to_i, 2, "Second way node ref.")
  assert(nds[2]['ref'].to_i, 3, "Third way node ref.")
  assert(nds[3]['ref'].to_i, 1, "Fourth way node ref.")

  tags = Hash[node.children.select {|t| t.element? && t.name == "tag"}.map {|t| [t['k'],t['v']]}]
  assert(tags.size, 2, "Number of tags.")
  assert(tags['highway'], 'motorway', "First tag.")
  assert(tags['name'], 'Foo', "Second tag.")
end

test_request("GET", "/api/0.6/node/2/ways", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 2, "Number of children of the <osm> element.")
  node = children[0]
  assert(node.name, "node", "Name of <osm> child.")
  assert(node["id"].to_i, 2, "ID of node")
  assert(node["lat"], "1.0000000", "Latitude attribute")
  assert(node["lon"], "1.0000000", "Longitude attribute")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], "1", "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 8, "Version attribute")
  assert(node["changeset"].to_i, 3, "Changeset ID")
  assert(node["timestamp"], "2012-10-01T00:00:00Z", "Timestamp")
  tags = Hash[node.children.select {|t| t.element?}.map {|t| [t['k'],t['v']]}]
  assert(tags.size, 3, "Number of tags.")
  assert(tags['foo'], 'bar1', "First tag.")
  assert(tags['bar'], 'bar2', "Second tag.")
  assert(tags['baz'], 'bar3', "Third tag.")
  node = children[1]

  assert(node.name, "way", "Name of <osm> child.")
  assert(node["id"].to_i, 1, "ID of way")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], "1", "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 1, "Version attribute")
  assert(node["changeset"].to_i, 1, "Changeset ID")
  assert(node["timestamp"], "2012-12-01T00:00:00Z", "Timestamp")

  nds = node.children.select {|n| n.element? && n.name == "nd" }
  assert(nds.size, 4, "Number of way nodes.")
  assert(nds[0]['ref'].to_i, 1, "First way node ref.")
  assert(nds[1]['ref'].to_i, 2, "Second way node ref.")
  assert(nds[2]['ref'].to_i, 3, "Third way node ref.")
  assert(nds[3]['ref'].to_i, 1, "Fourth way node ref.")

  tags = Hash[node.children.select {|t| t.element? && t.name == "tag"}.map {|t| [t['k'],t['v']]}]
  assert(tags.size, 2, "Number of tags.")
  assert(tags['highway'], 'motorway', "First tag.")
  assert(tags['name'], 'Foo', "Second tag.")end

test_request("GET", "/api/0.6/node/4/ways", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "410 Gone", "Response status code.")
end

test_request("GET", "/api/0.6/node/5/ways", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers['Status'], "404 Not Found", "Response status code.")
end

# node call returns not found if the node number isn't a number
test_request('GET', '/api/0.6/node/five/ways', 'HTTP_ACCEPT' => 'text/xml') do |headers, data|
  assert(headers['Status'], '404 Not Found', 'Response status code.')
end
