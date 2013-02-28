#!/usr/bin/env ruby

require 'xml/libxml'
$LOAD_PATH << File.dirname(__FILE__)
require 'test_functions.rb'

[nil,        # no Accept header
 '*/*',      # Accept: */*
 'text/xml', # Accept: text/xml
 'text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2' # Nonsense from JOSM
].each do |accept|
  test_request("GET", "/api/0.6/map?bbox=0,0,0,0", "HTTP_ACCEPT" => accept) do |headers, data|
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
end
