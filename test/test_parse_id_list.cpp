#include "cgimap/api06/handler_utils.hpp"
#include "cgimap/api06/id_version_io.hpp"
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

  boost::posix_time::ptime get_current_time() const {
    return boost::posix_time::ptime();
  }

protected:
  virtual void write_header_info(int status, const headers_t &headers) {}
  virtual boost::shared_ptr<output_buffer> get_buffer_internal() {
    return boost::shared_ptr<output_buffer>();
  }
  virtual void finish_internal() {}

private:
  std::map<std::string, std::string> m_params;
};

std::vector<api06::id_version> parse_query_str(std::string query_str) {
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
  std::vector<api06::id_version> ids = parse_query_str(query_str);

  if (ids.size() != 1) {
    std::ostringstream err;
    err << "Parsing " << query_str << " as a list of nodes should "
        << "discard duplicates, but got: {";
    BOOST_FOREACH(api06::id_version id, ids) {
      err << id << ", ";
    }
    err << "}\n";
    throw std::runtime_error(err.str());
  }
}

void test_parse_negative_nodes() {
  std::string query_str =
    "nodes=-1875196430,1970729486,-715595887,153329585,276538320,276538320,"
    "276538320,276538320,557671215,268800768,268800768,272134694,416571249,"
    "4118507737,639141976,-120408340,4118507737,4118507737,-176459559,"
    "-176459559,-176459559,416571249,-176459559,-176459559,-176459559,"
    "557671215";

  std::vector<api06::id_version> ids = parse_query_str(query_str);

  // maximum ID that postgres can handle is 2^63-1, so that should never
  // be returned by the parsing function.
  const osm_nwr_id_t max_id = std::numeric_limits<int64_t>::max();
  BOOST_FOREACH(api06::id_version idv, ids) {
    if (idv.id > max_id) {
      throw std::runtime_error("Found ID > max allowed ID in parsed list.");
    }
  }
}

void test_parse_history() {
  std::string query_str = "nodes=1,1v1";

  std::vector<api06::id_version> ids = parse_query_str(query_str);

  if (ids.size() != 2) {
    throw std::runtime_error("Expected ID list of size 2.");
  }

  // NOTE: ID list is uniqued and sorted, which puts the "latest" version
  // at the end.
  if (ids[0] != api06::id_version(1, 1)) {
  std::ostringstream out;
    out << "Expected ID=1, version=1 to be the first element, but got "
        << ids[0] << ".";
    throw std::runtime_error(out.str());
  }

  if (ids[1] != api06::id_version(1)) {
    throw std::runtime_error("Expected ID=1, latest version to be the second element.");
  }
}

} // anonymous namespace

int main(int argc, char *argv[]) {
  try {
    test_parse_returns_no_duplicates();
    test_parse_negative_nodes();
    test_parse_history();

  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;

  } catch (...) {
    std::cout << "Unknown exception type." << std::endl;
    return 99;
  }

  return 0;
}
