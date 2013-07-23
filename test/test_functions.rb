require 'open3'
require 'xml/libxml'
require 'date'

class AssertionError < StandardError
  attr_reader :message

  def initialize(message)
    @message = message
  end
end

def assert(actual, expected, message = nil)
  unless actual == expected
    raise AssertionError.new("Expected #{expected.inspect}, got #{actual.inspect}: #{message.inspect}")
  end
end

def test_request(method, uri, env = {})
  env["REMOTE_ADDR"] = "127.0.0.1" unless env.has_key? "REMOTE_ADDR"
  env["HTTP_ACCEPT"] = "*/*" unless env.has_key? "HTTP_ACCEPT"
  env["REQUEST_METHOD"] = method
  env["REQUEST_URI"] = uri
  
  Open3.popen3(env, "cgi-fcgi", "-bind", "-connect", ARGV[0]) do |sin,sout,serr,thr|
    sin.close
    headers, data = sout.read.split("\r\n\r\n")
    assert(headers.nil?, false, "FCGI should have returned headers.")
    headers = Hash[*headers.split("\r\n").collect_concat{|l| k,v = l.split(": "); [k,v]}]
    
    assert(thr.value.success?, true, "FCGI exited successfully.")
    
    begin
      yield(headers, data)
    rescue AssertionError => e
      STDERR.puts "ERROR: #{e.message}"
      STDERR.puts "HEADERS: #{headers.inspect}"
      STDERR.puts "BODY: #{data.inspect}"
      exit 1
    end
  end
end

GEO_SCALE = 10000000

## from the rails port sources
def tile_for_point(lat, lon)
  x = ((lon.to_f + 180) * 65535 / 360).round
  y = ((lat.to_f + 90) * 65535 / 180).round
  
  return tile_for_xy(x, y)
end

## from the rails port sources
def self.tile_for_xy(x, y)
  t = 0
  
  16.times do
    t = t << 1
    t = t | 1 unless (x & 0x8000).zero?
    x <<= 1
    t = t << 1
    t = t | 1 unless (y & 0x8000).zero?
    y <<= 1
  end
  
  return t
end

def load_osm_file(file_name, conn)
  doc = XML::Parser.file(file_name).parse

  # extract the users and changesets for setting up the users table
  users = Hash.new
  changesets = Hash.new
  ['node', 'way', 'relation'].each do |type|
    doc.find(type).each do |n|
      uid = (n["uid"] || "-1").to_i
      csid = n["changeset"].to_i
      
      timestamp = DateTime.strptime(n["timestamp"], "%Y-%m-%dT%H:%M:%S%Z")
      u_timestamp = users.has_key?(uid) ? [timestamp, users[uid][:timestamp]].min : timestamp
      cs_min_timestamp = changesets.has_key?(csid) ? [timestamp, changesets[csid][:min_timestamp]].min : timestamp
      cs_max_timestamp = changesets.has_key?(csid) ? [timestamp, changesets[csid][:max_timestamp]].max : timestamp
      cs_num_changes = changesets.has_key?(csid) ? changesets[csid][:num_changes] + 1 : 1
      
      users[uid] = { :display_name => n["user"], :timestamp => u_timestamp }
      changesets[csid] = { :uid => uid, :min_timestamp => cs_min_timestamp, :max_timestamp => cs_max_timestamp, :num_changes => cs_num_changes }
    end
  end

  users.each do |uid,data|
    conn.exec("insert into users(id,email,pass_crypt,creation_time,display_name,data_public) values (#{uid},'user_#{uid}@example.com','','#{data[:timestamp]}','#{data[:display_name]}',true)")
  end

  changesets.each do |csid,data|
    conn.exec("insert into changesets (id, user_id, created_at, min_lat, max_lat, min_lon, max_lon, closed_at, num_changes) values (#{csid}, #{data[:uid]}, '#{data[:min_timestamp]}', 0, 0, 0, 0, '#{data[:max_timestamp]}', #{data[:num_changes]})")
  end

  doc.find("node").each do |n|
    nid = n["id"].to_i
    uid = n["uid"].to_i
    csid = n["changeset"].to_i
    lon = (n["lon"].to_f * GEO_SCALE).to_i
    lat = (n["lat"].to_f * GEO_SCALE).to_i
    visible = n["visible"] == "true"
    version = n["version"].to_i
    timestamp = DateTime.strptime(n["timestamp"], "%Y-%m-%dT%H:%M:%S%Z")
    tile = tile_for_point(n["lat"], n["lon"])

    conn.exec("insert into current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) values (#{nid}, #{lat}, #{lon}, #{csid}, #{visible}, '#{timestamp}', #{tile}, #{version})")

    n.find("tag").each do |t|
      k = t['k']
      v = t['v']
      conn.exec("insert into current_node_tags (node_id, k, v) values (#{nid}, '#{k}', '#{v}')");
    end
  end

  doc.find("way").each do |w|
    wid = w["id"].to_i
    uid = w["uid"].to_i
    csid = w["changeset"].to_i
    visible = w["visible"] == "true"
    version = w["version"].to_i
    timestamp = DateTime.strptime(w["timestamp"], "%Y-%m-%dT%H:%M:%S%Z")

    conn.exec("insert into current_ways (id, changeset_id, visible, \"timestamp\", version) values (#{wid}, #{csid}, #{visible}, '#{timestamp}', #{version})")

    w.find("tag").each do |t|
      k = t['k']
      v = t['v']
      conn.exec("insert into current_way_tags (way_id, k, v) values (#{wid}, '#{k}', '#{v}')")
    end

    w.find("nd").each_with_index do |wn,ix|
      ref = wn['ref'].to_i
      conn.exec("insert into current_way_nodes (way_id, node_id, sequence_id) values (#{wid}, #{ref}, #{ix})")
    end
  end

  doc.find("relation").each do |r|
    rid = r['id'].to_i
    uid = r['uid'].to_i
    csid = r['changeset'].to_i
    visible = r['visible'] == 'true'
    version = r['version'].to_i
    timestamp = DateTime.strptime(r['timestamp'], '%Y-%m-%dT%H:%M:%S%Z')

    conn.exec("insert into current_relations (id, changeset_id, visible, \"timestamp\", version) values (#{rid}, #{csid}, #{visible}, '#{timestamp}', #{version})")

    r.find('tag').each do |t|
      k = t['k']
      v = t['v']
      conn.exec("insert into current_relation_tags (relation_id, k, v) values (#{rid}, '#{k}', '#{v}')")
    end

    r.find('member').each_with_index do |rm,ix|
      type = rm['type'].capitalize
      role = rm['role']
      ref = rm['ref'].to_i
      conn.exec("insert into current_relation_members (relation_id, member_type, member_id, member_role, sequence_id) values (#{rid}, '#{type}'::nwr_enum, #{ref}, '#{role}', #{ix})")
    end
  end
end
