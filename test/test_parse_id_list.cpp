#include "cgimap/api06/handler_utils.hpp"
#include <boost/foreach.hpp>
#include <iostream>
#include <sstream>
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

std::vector<osm_nwr_id_t> parse_query_str(std::string query_str) {
  test_request req;
  req.set_header("REQUEST_METHOD", "GET");
  req.set_header("QUERY_STRING", query_str);
  req.set_header("PATH_INFO", "/api/0.6/nodes");

  return api06::parse_id_list_params(req, "nodes");
}

void test_parse_returns_no_duplicates() {
  std::string query_str = "nodes=1,1,1,1";

  // the container returned from parse_id_list_params should not contain
  // any duplicates.
  std::vector<osm_nwr_id_t> ids = parse_query_str(query_str);

  if (ids.size() != 1) {
    std::ostringstream err;
    err << "Parsing " << query_str << " as a list of nodes should "
        << "discard duplicates, but got: {";
    BOOST_FOREACH(osm_nwr_id_t id, ids) {
      err << id << ", ";
    }
    err << "}\n";
    throw std::runtime_error(err.str());
  }
}

} // anonymous namespace

int main(int argc, char *argv[]) {
  try {
    test_parse_returns_no_duplicates();

  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cout << "Unknown exception type." << std::endl;
    return 99;
  }

  return 0;
}
