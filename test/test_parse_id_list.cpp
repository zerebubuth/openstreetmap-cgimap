#include "cgimap/api06/handler_utils.hpp"
#include <boost/foreach.hpp>
#include <string>
#include <map>

namespace {

struct test_request : public request {
  test_request() {}

  /// implementation of request interface
  virtual ~test_request() {}
  virtual const char *get_param(const char *key) {
    std::string key_str(key);
    std::map<std::string, std::string>::iterator itr = m_params.find(key_str);
    if (itr != m_params.end()) {
      return itr->second.c_str();
    } else {
      return NULL;
    }
  }

  virtual void dispose() {}

  /// getters and setters for the input headers and output response
  void set_header(const std::string &k, const std::string &v) {
    m_params.insert(std::make_pair(k, v));
  }
  std::stringstream &buffer() { assert(false); }

protected:
  virtual void write_header_info(int status, const headers_t &headers) {}
  virtual boost::shared_ptr<output_buffer> get_buffer_internal() {
    return boost::shared_ptr<output_buffer>();
  }
  virtual void finish_internal() {}

private:
  std::map<std::string, std::string> m_params;
};

} // anonymous namespace

int main(int argc, char *argv[]) {
  std::string query_str = "nodes=1,1,1,1";
  test_request req;
  req.set_header("REQUEST_METHOD", "GET");
  req.set_header("QUERY_STRING", query_str);
  req.set_header("PATH_INFO", "/api/0.6/nodes");

  // the container returned from parse_id_list_params should not contain
  // any duplicates.
  std::vector<osm_id_t> ids = api06::parse_id_list_params(req, "nodes");
  if (ids.size() != 1) {
    std::cerr << "Parsing " << query_str << " as a list of nodes should "
              << "discard duplicates, but got: {";
    BOOST_FOREACH(osm_id_t id, ids) {
      std::cerr << id << ", ";
    }
    std::cerr << "}\n";
    return 1;
  }

  return 0;
}
