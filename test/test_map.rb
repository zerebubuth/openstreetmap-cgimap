#!/usr/bin/env ruby

require 'xml/libxml'
require 'set'
require 'pg'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

conn = PG.connect(dbname: ARGV[1])
load_osm_file("#{File.dirname(__FILE__)}/test_map.osm", conn)

# bad requests:
['/api/0.6/map',                          # no bbox parameter
 '/api/0.6/map?bbox=',                    # empty bbox parameter
 '/api/0.6/map?bbox=0,0,0',               # not enough numbers
 '/api/0.6/map?bbox=0.1,0.1,-0.1,-0.1',   # negative size bbox
 '/api/0.6/map?bbox=179.9,0,180.1,0.1',   # bbox out of valid range
 '/api/0.6/map?bbox=-180.1,0,-179.9,0.1', # bbox out of valid range
 '/api/0.6/map?bbox=0,89.9,0.1,90.1',     # bbox out of valid range
 '/api/0.6/map?bbox=0,-90.1,0.1,-89.9'    # bbox out of valid range
].each do |req|
  test_request('GET', req, 'HTTP_ACCEPT' => 'text/xml') do |headers, data|
    assert(headers['Status'], '400 Bad Request', "Response status code [#{req}].")
  end
end
