require 'open3'

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
    headers = Hash[*headers.split("\r\n").collect_concat{|l| l.split(": ")}]
    
    assert(thr.value.success?, true, "FCGI exited successfully.")
    yield(headers, data)
  end
end
