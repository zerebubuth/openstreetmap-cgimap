#!/usr/bin/env ruby

require 'xml/libxml'
require 'json'
require 'set'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
load_osm_file("#{File.dirname(__FILE__)}/test_node.osm", conn)

test_request("GET", "/api/0.6/node/1", "HTTP_ACCEPT" => "text/json") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/json; charset=utf-8", "Response content type.")

  doc = JSON.load(data)
  assert(doc.has_key?('nodes'), true, "Document has a 'nodes' key.")
  nodes = doc['nodes']
  assert(nodes.class, Array, "Nodes element of document is an array.")
  assert(nodes.size, 1, "Number of nodes.")
  node = nodes[0]
  assert(node["id"], 1, "ID of node")
  assert(node["lat"], 0.0, "Latitude attribute")
  assert(node["lon"], 0.0, "Longitude attribute")
  assert(node["user"], "foo", "User name")
  assert(node["uid"], 1, "User ID")
  assert(node["visible"], true, "Visibility attribute")
  assert(node["version"], 1, "Version attribute")
  assert(node["changeset"], 1, "Changeset ID")
  assert(node["timestamp"], "2012-09-25T00:00:00Z", "Timestamp")
  # node 1 has no tags, but should still have "tags" object which
  # will be empty.
  assert(node.has_key?("tags"), true, "Node without tags has key 'tags'.")
  assert(node["tags"].empty?, true, "Node without tags has empty 'tags' object.")
end

# grab an empty bounding box and check it has empty nodes, ways &
# relations arrays.
test_request("GET", "/api/0.6/map?bbox=10,10,10.1,10.1", "HTTP_ACCEPT" => "text/json") do |headers, data|
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/json; charset=utf-8", "Response content type.")

  doc = JSON.load(data)

  ['nodes', 'ways', 'relations'].each do |name|
    assert(doc.has_key?(name), true, "Document has a '#{name}' key.")
    array = doc[name]
    assert(array.class, Array, "Element '#{name}' of document is an array.")
    assert(array.size, 0, "Number of #{name}s.")
  end
end

# TODO - add more tests!
