#!/usr/bin/env ruby

require 'xml/libxml'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
load_osm_file("#{File.dirname(__FILE__)}/test_way.osm", conn)

test_request("GET", "/api/0.6/way/1", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 1, "Number of children of the <osm> element.")
  node = children[0]

  assert(node.name, "way", "Name of <osm> child.")
  assert(node["id"].to_i, 1, "ID of node")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], "1", "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 1, "Version attribute")
  assert(node["changeset"].to_i, 1, "Changeset ID")
  assert(node["timestamp"], "2012-12-01T00:00:00Z", "Timestamp")

  nds = node.children.select {|n| n.element? && n.name == "nd" }
  assert(nds.size, 3, "Number of way nodes.")
  assert(nds[0]['ref'].to_i, 1, "First way node ref.")
  assert(nds[1]['ref'].to_i, 2, "Second way node ref.")
  assert(nds[2]['ref'].to_i, 3, "Third way node ref.")

  tags = Hash[node.children.select {|t| t.element? && t.name == "tag"}.map {|t| [t['k'],t['v']]}]
  assert(tags.size, 2, "Number of tags.")
  assert(tags['highway'], 'motorway', "First tag.")
  assert(tags['name'], 'Foo', "Second tag.")
end

test_request("GET", "/api/0.6/way/2", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "410 Gone", "Response status code.")
end

test_request("GET", "/api/0.6/way/3", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers['Status'], "404 Not Found", "Response status code.")
end
