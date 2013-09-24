#!/usr/bin/env ruby

require 'xml/libxml'
require 'set'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
load_osm_file("#{File.dirname(__FILE__)}/test_relation.osm", conn)

# relation/full returns the relation, plus all the unique members, plus nodes
# which are part of included ways.
test_request("GET", "/api/0.6/relation/1/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")
  assert(headers.has_key?("Content-Disposition"), false, "Response content disposition header.")

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

# utility function for relation/full requests
def test_full_request(id, elems)
  test_request("GET", "/api/0.6/relation/#{id}/full", "HTTP_ACCEPT" => "text/xml") do |headers, data|
    assert(headers["Status"], "200 OK", "Response status code [#{id}].")
    assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type [#{id}].")
    
    doc = XML::Parser.string(data).parse
    assert(doc.root.name, "osm", "Document root element [#{id}].")
    children = doc.root.children.select {|n| n.element?}
    
    # elements should be returned in node, way, relation order
    assert(children.map {|n| n.name}, elems.map {|e| e[0]}, "Element types [#{id}].")
    # check the element IDs
    assert(children.map {|n| n.name + "/" + n['id']}.sort, elems.map {|e| e[0] + "/" + e[1].to_s}.sort, "Element types and IDs [#{id}].")
  end
end

# relation 4 just contains another relation, which should be included but its
# members shouldn't be.
test_full_request(4, [['relation', 1], ['relation', 4]])

# relation 5 contains itself, but shouldn't be recursed upon
test_full_request(5, [['relation', 5]])

# relation 6 just contains the way, but /full should return way nodes too
test_full_request(6, [['node',1], ['node',2], ['way',1], ['relation', 6]])

# relation 7 contains relation 8 (which contains relation 7 (which ... and so
# on)) but this mutual recursion shouldn't cause us a problem.
test_full_request(7, [['relation', 7], ['relation', 8]])
