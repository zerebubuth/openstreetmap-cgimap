#!/usr/bin/env ruby

require 'open3'
require 'xml/libxml'

def assert(actual, expected, message = nil)
  unless actual == expected
    STDERR.puts("Expected #{expected.inspect}, got #{actual.inspect}: #{message}") unless message.nil?
    exit 1
  end
end

env = { 
  "REMOTE_ADDR" => "127.0.0.1",
  "REQUEST_URI" => "/api/0.6/map?bbox=0,0,0,0",
  "REQUEST_METHOD" => "GET",
  "HTTP_ACCEPT" => "text/xml"
}

Open3.popen3(env, "cgi-fcgi", "-bind", "-connect", ARGV[0]) do |sin,sout,serr,thr|
  sin.close
  headers, data = sout.read.split("\r\n\r\n")
  headers = Hash[*headers.split("\r\n").collect_concat{|l| l.split(": ")}]

  assert(thr.value.success?, true, "FCGI exited successfully.")
  assert(headers["Status"], "200 OK", "Response status code.")
  assert(headers["Content-Type"], "text/xml; charset=utf-8", "Response content type.")

  doc = XML::Parser.string(data).parse
  assert(doc.root.name, "osm", "Document root element.")
  children = doc.root.children.select {|n| n.element?}
  assert(children.size, 1, "Number of children of the <osm> element.")
  assert(children[0].name, "bounds", "Name of <osm> child.")
  ["minlon", "minlat", "maxlon", "maxlat"].each do |attr|
    assert(children[0][attr].to_f, 0.0, "Value of #{attr} attribute of <bounds> element.")
  end
end

