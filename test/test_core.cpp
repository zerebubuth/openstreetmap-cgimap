#include "process_request.hpp"
#include "backend/staticxml/staticxml.hpp"
#include "config.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <sstream>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace al = boost::algorithm;

/**
 * Mock output buffer so that we can get back an in-memory result as a string
 * backed buffer.
 */
struct test_output_buffer : public output_buffer {
  explicit test_output_buffer(std::ostream &out) : m_out(out), m_written(0) {}
  virtual ~test_output_buffer() {}
  virtual int write(const char *buffer, int len) { m_out.write(buffer, len); m_written += len; return len; }
  virtual int written() { return m_written; }
  virtual int close() { return 0; }
  virtual void flush() {}
private:
  std::ostream &m_out;
  int m_written;
};

/**
 * Mock request so that we can control the headers and get back the response
 * body for comparison to what we expect.
 */
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
  virtual int put(const char *data, int len) { m_output.write(data, len); return len; }
  virtual int put(const std::string &s) { m_output.write(s.c_str(), s.size()); return s.size(); }
  virtual boost::shared_ptr<output_buffer> get_buffer() {
    return boost::shared_ptr<output_buffer>(new test_output_buffer(m_output));
  }
  virtual std::string cors_headers() { return ""; }
  virtual void flush() {}
  virtual int accept_r() { return 0; }
  virtual void finish() {}

  /// getters and setters for the input headers and output response
  void set_header(const std::string &k, const std::string &v) {
    m_params.insert(std::make_pair(k, v));
  }
  std::stringstream &buffer() { return m_output; }

private:
  std::stringstream m_output;
  std::map<std::string, std::string> m_params;
};

std::map<std::string, std::string> read_headers(std::istream &in, const std::string &separator) {
  std::map<std::string, std::string> headers;

  while (true) {
    std::string line;
    std::getline(in, line);
    al::erase_all(line, "\r");
    if (!in.good()) { throw std::runtime_error("Test file ends before separator."); }
    if (line == separator) { break; }

    boost::iterator_range<std::string::iterator> result = al::find_first(line, ":");
    if (!result) { throw std::runtime_error("Test file header doesn't match expected format."); }

    std::string key(line.begin(), result.begin());
    std::string val(result.end(), line.end());
    
    al::trim(key);
    al::trim(val);

    headers.insert(std::make_pair(key, val));
  }

  return headers;
}

/**
 * take the test file and use it to set up the request headers.
 */
void setup_request_headers(test_request &req, std::istream &in) {
  typedef std::map<std::string, std::string> dict;
  dict headers = read_headers(in, "---");

  BOOST_FOREACH(const dict::value_type &val, headers) {
    std::string key(val.first);
    
    al::to_upper(key);
    al::replace_all(key, "-", "_");

    req.set_header(key, val.second);
  }

  // always set the remote addr variable
  req.set_header("REMOTE_ADDR", "127.0.0.1");
}

/**
 * Check the response from cgimap against the expected test result
 * from the test file.
 */
void check_response(std::istream &expected, std::istream &actual) {
  typedef std::map<std::string, std::string> dict;
  const dict expected_headers = read_headers(expected, "---");
  const dict actual_headers = read_headers(actual, "");

  BOOST_FOREACH(const dict::value_type &val, actual_headers) {
    std::cerr << val.first << ": " << val.second << std::endl;
  }

  BOOST_FOREACH(const dict::value_type &val, expected_headers) {
    dict::const_iterator itr = actual_headers.find(val.first);
    if (itr == actual_headers.end()) {
      throw std::runtime_error((boost::format("Expected header `%1%: %2%', but didn't find it in actual response.")
                                % val.first % val.second).str());
    }
    if (!val.second.empty()) {
      if (val.second != itr->second) {
        throw std::runtime_error((boost::format("Header key `%1%'; expected `%2%' but got `%3%'.")
                                  % val.first % val.second % itr->second).str());
      }
    }
  }
}

/**
 * Main test body:
 *  - reads the test case,
 *  - constructs a matching mock request,
 *  - executes it through the standard process_request() chain,
 *  - compares the result to what's expected in the test case.
 */
void run_test(fs::path test_case,
              rate_limiter &limiter, const std::string &generator, routes &route,
              boost::shared_ptr<data_selection::factory> factory) {
  try {
    test_request req;
    
    // set up request headers from test case
    fs::ifstream in(test_case);
    setup_request_headers(req, in);
    
    // execute the request
    process_request(req, limiter, generator, route, factory);
    
    // compare the result to what we're expecting
    check_response(in, req.buffer());

  } catch (const std::exception &ex) {
    throw std::runtime_error((boost::format("%1%, in %2% test.") % ex.what() % test_case).str());
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <test-directory>." << std::endl;
    return 99;
  }

  fs::path test_directory = argv[1];
  fs::path data_file = test_directory / "data.osm";
  std::vector<fs::path> test_cases;

  try {
    if (fs::is_directory(test_directory) == false) {
      std::cerr << "Test directory " << test_directory << " should be a directory, but isn't.";
      return 99;
    }
    if (fs::is_regular_file(data_file) == false) {
      std::cerr << "Test directory should contain data file at " << data_file << ", but does not.";
      return 99;
    }
    const fs::directory_iterator end;
    for (fs::directory_iterator itr(test_directory); itr != end; ++itr) {
      fs::path filename = itr->path();
      std::string ext = fs::extension(filename);
      if (ext == ".case") {
        test_cases.push_back(filename);
      }
    }

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 99;
  
  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 99;
  }

  try {
    po::variables_map vm;
    vm.insert(std::make_pair(std::string("file"), po::variable_value(data_file.native(), false)));

    boost::shared_ptr<backend> data_backend = make_staticxml_backend();
    boost::shared_ptr<data_selection::factory> factory = data_backend->create(vm);
    rate_limiter limiter(vm);
    routes route;

    BOOST_FOREACH(fs::path test_case, test_cases) {
      std::string generator = (boost::format(PACKAGE_STRING " (test %1%)") % test_case).str();
      run_test(test_case, limiter, generator, route, factory);
    }

  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  
  } catch (...) {
    std::cerr << "UNKNOWN EXCEPTION" << std::endl;
    return 1;
  }

  return 0;
}
