#!/usr/bin/env ruby

require 'xml/libxml'
require 'set'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
load_osm_file("#{File.dirname(__FILE__)}/test_way.osm", conn)

# way/full returns the way, plus all the unique nodes
test_request("GET", "/api/0.6/way/1/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")
  assert(headers.has_key?("Content-Disposition"), false, "Response content disposition header.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 4, "Number of children of the <osm> element.")

  # doc should return nodes first, but not necessarily in sorted order
  assert(children[0].name, "node", "Name of first <osm> child")
  assert(children[1].name, "node", "Name of second <osm> child")
  assert(children[2].name, "node", "Name of third <osm> child")

  assert([0,1,2].map {|i| children[i]['id'].to_i}.sort, [1,2,3], 'IDs of nodes')

  # then it should return the way
  assert(children[3].name, "way", "Name of fourth <osm> child")
  assert(children[3]["id"].to_i, 1, "ID of way")
  assert(children[3]["user"], "foo", "User name")
  assert(children[3]["uid"], "1", "User ID")
  assert(children[3]["visible"], "true", "Visibility attribute")
  assert(children[3]["version"].to_i, 1, "Version attribute")
  assert(children[3]["changeset"].to_i, 1, "Changeset ID")
  assert(children[3]["timestamp"], "2012-12-01T00:00:00Z", "Timestamp")

  nds = children[3].children.select {|n| n.element? && n.name == "nd" }
  assert(nds.size, 4, "Number of way nodes.")
  assert(nds[0]['ref'].to_i, 1, "First way node ref.")
  assert(nds[1]['ref'].to_i, 2, "Second way node ref.")
  assert(nds[2]['ref'].to_i, 3, "Third way node ref.")
  assert(nds[3]['ref'].to_i, 1, "Fourth way node ref.")

  tags = Hash[children[3].children.select {|t| t.element? && t.name == "tag"}.map {|t| [t['k'],t['v']]}]
  assert(tags.size, 2, "Number of tags.")
  assert(tags['highway'], 'motorway', "First tag.")
  assert(tags['name'], 'Foo', "Second tag.")
end

# way/full on a deleted way should say 'gone'
test_request("GET", "/api/0.6/way/2/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "410 Gone", "Response status code.")
end

# way/full on a way that never existed should say 'not found'
test_request("GET", "/api/0.6/way/3/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers['Status'], "404 Not Found", "Response status code.")
end

