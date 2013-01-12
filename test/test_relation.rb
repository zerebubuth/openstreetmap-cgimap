#!/usr/bin/env ruby

require 'xml/libxml'
require 'set'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
load_osm_file("#{File.dirname(__FILE__)}/test_relation.osm", conn)

# test that a relation get returns OK and the content is what 
# we expected.
test_request("GET", "/api/0.6/relation/1", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 1, "Number of children of the <osm> element.")
  node = children[0]

  assert(node.name, "relation", "Name of <osm> child.")
  assert(node["id"].to_i, 1, "ID of node")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], "1", "User ID")
  assert(node["visible"], "true", "Visibility attribute")
  assert(node["version"].to_i, 1, "Version attribute")
  assert(node["changeset"].to_i, 1, "Changeset ID")
  assert(node["timestamp"], "2012-12-01T00:00:00Z", "Timestamp")
end

# a deleted relation should say 'gone'
test_request("GET", "/api/0.6/relation/2", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "410 Gone", "Response status code.")
end

# a relation that has never existed should say 'not found'
test_request("GET", "/api/0.6/relation/3", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers['Status'], "404 Not Found", "Response status code.")
end

# relations call returns all relations, even deleted ones, in the request
test_request("GET", "/api/0.6/relations?relations=1,2", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 2, "Number of children of the <osm> element.")
  
  ids = Set.new(children.map {|n| assert(n.name, "relation", "Name of <osm> child."); n['id'].to_i })
  assert(ids, Set.new([1,2]), "IDs of relations.")
end

# however, the whole thing returns 404 if just one of the relations can't be found
test_request("GET", "/api/0.6/relations?relations=1,2,3", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers['Status'], "404 Not Found", "Response status code.")  
end

# relations call returns bad request if the list of relations is empty
test_request("GET", "/api/0.6/relations?relations=", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers['Status'], "400 Bad Request", "Response status code.")
end

# relation/full returns the relation, plus all the unique members, plus nodes
# which are part of included ways.
test_request("GET", "/api/0.6/relation/1/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}

  # elements should be returned in node, way, relation order
  assert(children.map {|n| n.name}, ['node', 'node', 'way', 'relation'], "Element types")
  # check the element IDs
  assert(children.map {|n| n.name + "/" + n['id']}.sort, ['node/1', 'node/2', 'way/1', 'relation/1'].sort, "Element types and IDs")
end

# a deleted relation/full should say 'gone', same as plain relation get
test_request("GET", "/api/0.6/relation/2/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "410 Gone", "Response status code.")
end

# a relation/full that has never existed should say 'not found', same as plain relation get
test_request("GET", "/api/0.6/relation/3/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers['Status'], "404 Not Found", "Response status code.")
end
