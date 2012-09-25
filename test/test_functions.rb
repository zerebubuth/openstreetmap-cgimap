require 'open3'
require 'xml/libxml'
require 'date'

def assert(actual, expected, message = nil)
  unless actual == expected
    STDERR.puts("Expected #{expected.inspect}, got #{actual.inspect}: #{message}") unless message.nil?
    exit 1
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
    headers = Hash[*headers.split("\r\n").collect_concat{|l| k,v = l.split(": "); [k,v]}]
    
    assert(thr.value.success?, true, "FCGI exited successfully.")
    yield(headers, data)
  end
end

GEO_SCALE = 10000000

def load_osm_file(file_name, conn)
  doc = XML::Parser.file(file_name).parse

  # extract the users and changesets for setting up the users table
  users = Hash.new
  changesets = Hash.new
  doc.find("node").each do |n|
    uid = n["uid"].to_i
    csid = n["changeset"].to_i

    timestamp = DateTime.strptime(n["timestamp"], "%Y-%m-%dT%H:%M:%S%Z")
    u_timestamp = users.has_key?(uid) ? [timestamp, users[uid][:timestamp]].min : timestamp
    cs_min_timestamp = changesets.has_key?(csid) ? [timestamp, changesets[csid][:timestamp]].min : timestamp
    cs_max_timestamp = changesets.has_key?(csid) ? [timestamp, changesets[csid][:timestamp]].max : timestamp
    cs_num_changes = changesets.has_key?(csid) ? changesets[csid][:num_changes] + 1 : 1

    users[uid] = { :display_name => n["user"], :timestamp => u_timestamp }
    changesets[csid] = { :uid => uid, :min_timestamp => cs_min_timestamp, :max_timestamp => cs_max_timestamp, :num_changes => cs_num_changes }
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
    
    # TODO: node tile calculation
    conn.exec("insert into current_nodes (id, latitude, longitude, changeset_id, visible, \"timestamp\", tile, version) values (#{nid}, #{lat}, #{lon}, #{csid}, #{visible}, '#{timestamp}', 0, #{version})")
  end
end
