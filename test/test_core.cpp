#include "cgimap/process_request.hpp"
#include "cgimap/output_buffer.hpp"
#include "cgimap/backend/staticxml/staticxml.hpp"
#include "cgimap/request_helpers.hpp"
#include "cgimap/config.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <vector>
#include <sstream>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace al = boost::algorithm;
namespace pt = boost::property_tree;

/**
 * Mock output buffer so that we can get back an in-memory result as a string
 * backed buffer.
 */
struct test_output_buffer : public output_buffer {
  explicit test_output_buffer(std::ostream &out) : m_out(out), m_written(0) {}
  virtual ~test_output_buffer() {}
  virtual int write(const char *buffer, int len) {
    m_out.write(buffer, len);
    m_written += len;
    return len;
  }
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

  virtual void dispose() {}

  /// getters and setters for the input headers and output response
  void set_header(const std::string &k, const std::string &v) {
    m_params.insert(std::make_pair(k, v));
  }
  std::stringstream &buffer() { return m_output; }

protected:
  virtual void write_header_info(int status, const headers_t &headers) {
    assert(m_output.tellp() == 0);
    m_output << "Status: " << status << " " << status_message(status) << "\r\n";
    BOOST_FOREACH(const request::headers_t::value_type &header, headers) {
      m_output << header.first << ": " << header.second << "\r\n";
    }
    m_output << "\r\n";
  }
  virtual boost::shared_ptr<output_buffer> get_buffer_internal() {
    return boost::shared_ptr<output_buffer>(new test_output_buffer(m_output));
  }
  virtual void finish_internal() {}

private:
  std::stringstream m_output;
  std::map<std::string, std::string> m_params;
};

std::map<std::string, std::string> read_headers(std::istream &in,
                                                const std::string &separator) {
  std::map<std::string, std::string> headers;

  while (true) {
    std::string line;
    std::getline(in, line);

    // allow comments in lines which begin immediately with #. this shouldn't
    // conflict with any headers, as although http headers technically can start
    // with #, i'm pretty sure we're not using any which do.
    if ((line.size() > 0) && (line[0] == '#')) {
      continue;
    }

    al::erase_all(line, "\r");
    if (!in.good()) {
      throw std::runtime_error("Test file ends before separator.");
    }
    if (line == separator) {
      break;
    }

    boost::iterator_range<std::string::iterator> result =
        al::find_first(line, ":");
    if (!result) {
      throw std::runtime_error(
          "Test file header doesn't match expected format.");
    }

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
 * check the xml attributes of two elements are the same. this is a
 * different test method because we don't care about ordering for
 * attributes, whereas the main method for XML elements does.
 */
void check_xmlattr(const pt::ptree &expected, const pt::ptree &actual) {
  std::set<std::string> exp_keys, act_keys;

  BOOST_FOREACH(const pt::ptree::value_type &val, expected) {
    exp_keys.insert(val.first);
  }
  BOOST_FOREACH(const pt::ptree::value_type &val, actual) {
    act_keys.insert(val.first);
  }

  if (exp_keys.size() > act_keys.size()) {
    BOOST_FOREACH(const std::string &ak, act_keys) { exp_keys.erase(ak); }
    std::ostringstream out;
    out << "Missing attributes [";
    BOOST_FOREACH(const std::string &ek, exp_keys) { out << ek << " "; }
    out << "]";
    throw std::runtime_error(out.str());
  }

  if (act_keys.size() > exp_keys.size()) {
    BOOST_FOREACH(const std::string &ek, exp_keys) { act_keys.erase(ek); }
    std::ostringstream out;
    out << "Missing attributes [";
    BOOST_FOREACH(const std::string &ak, act_keys) { out << ak << " "; }
    out << "]";
    throw std::runtime_error(out.str());
  }

  BOOST_FOREACH(const std::string &k, exp_keys) {
    std::string exp_val = expected.get_child(k).data();
    std::string act_val = actual.get_child(k).data();
    if ((exp_val != act_val) && (exp_val != "***")) {
      throw std::runtime_error(
          (boost::format(
               "Attribute `%1%' expected value `%2%', but got `%3%'") %
           k % exp_val % act_val).str());
    }
  }
}

/**
 * recursively check an XML tree for a match. this is a very basic way of
 * doing it, but seems effective so far. the trees are walked depth-first
 * and values are compared exactly - except for when the expected value is
 * '***', which causes it to skip that subtree entirely.
 */
void check_recursive_tree(const pt::ptree &expected, const pt::ptree &actual) {
  pt::ptree::const_iterator exp_itr = expected.begin();
  pt::ptree::const_iterator act_itr = actual.begin();

  // skip comparison of trees for this wildcard.
  if (al::trim_copy(expected.data()) == "***") {
    return;
  }

  while (true) {
    if ((exp_itr == expected.end()) && (act_itr == actual.end())) {
      break;
    }
    if (exp_itr == expected.end()) {
      std::ostringstream out;
      out << "Actual result has more entries than expected: ["
          << act_itr->first;
      ++act_itr;
      while (act_itr != actual.end()) {
        out << ", " << act_itr->first;
        ++act_itr;
      }
      out << "] are extra";
      throw std::runtime_error(out.str());
    }
    if (act_itr == actual.end()) {
      std::ostringstream out;
      out << "Actual result has fewer entries than expected: ["
          << exp_itr->first;
      ++exp_itr;
      while (exp_itr != expected.end()) {
        out << ", " << exp_itr->first;
        ++exp_itr;
      }
      out << "] are absent";
      throw std::runtime_error(out.str());
    }
    if (exp_itr->first != act_itr->first) {
      throw std::runtime_error((boost::format("Expected %1%, but got %2%") %
                                exp_itr->first % act_itr->first).str());
    }
    try {
      if (exp_itr->first == "<xmlattr>") {
        check_xmlattr(exp_itr->second, act_itr->second);
      } else {
        check_recursive_tree(exp_itr->second, act_itr->second);
      }
    } catch (const std::exception &ex) {
      throw std::runtime_error((boost::format("%1%, in <%2%> element") %
                                ex.what() % exp_itr->first).str());
    }
    ++exp_itr;
    ++act_itr;
  }
}

/**
 * check that the content body of the expected, from the test case, and
 * actual, from the response, is the same.
 */
void check_content_body_xml(std::istream &expected, std::istream &actual) {
  pt::ptree exp_tree, act_tree;

  try {
    pt::read_xml(expected, exp_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        (boost::format("%1%, while reading expected XML.") % ex.what()).str());
  }

  try {
    pt::read_xml(actual, act_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        (boost::format("%1%, while reading actual XML.") % ex.what()).str());
  }

  // and check the results for equality
  check_recursive_tree(exp_tree, act_tree);
}

/**
 * recursively check a JSON tree for a match. this is a very basic way of
 * doing it, but seems effective so far. the trees are walked depth-first
 * and values are compared exactly - except for when the expected value is
 * '***', which causes it to skip that subtree entirely.
 */
void check_recursive_tree_json(const pt::ptree &expected,
                               const pt::ptree &actual) {
  pt::ptree::const_iterator exp_itr = expected.begin();
  pt::ptree::const_iterator act_itr = actual.begin();

  // skip comparison of trees for this wildcard.
  if (al::trim_copy(expected.data()) == "***") {
    std::cout << "wildcard\n";
    return;
  }

  // check the actual data value
  if (expected.data() != actual.data()) {
    throw std::runtime_error((boost::format("Expected '%1%', but got '%2%'") %
                              expected.data() % actual.data()).str());
  }
  std::cout << "attr match: " << expected.data() << "\n";

  while (true) {
    if ((exp_itr == expected.end()) && (act_itr == actual.end())) {
      break;
    }
    if (exp_itr == expected.end()) {
      std::ostringstream out;
      out << "Actual result has more entries than expected: ["
          << act_itr->first;
      ++act_itr;
      while (act_itr != actual.end()) {
        out << ", " << act_itr->first;
        ++act_itr;
      }
      out << "] are extra";
      throw std::runtime_error(out.str());
    }
    if (act_itr == actual.end()) {
      std::ostringstream out;
      out << "Actual result has fewer entries than expected: ["
          << exp_itr->first;
      ++exp_itr;
      while (exp_itr != expected.end()) {
        out << ", " << exp_itr->first;
        ++exp_itr;
      }
      out << "] are absent";
      throw std::runtime_error(out.str());
    }
    if (exp_itr->first != act_itr->first) {
      throw std::runtime_error((boost::format("Expected %1%, but got %2%") %
                                exp_itr->first % act_itr->first).str());
    }
    try {
      std::cout << "recursing on item " << exp_itr->first << "\n";
      check_recursive_tree_json(exp_itr->second, act_itr->second);
    } catch (const std::exception &ex) {
      throw std::runtime_error((boost::format("%1%, in \"%2%\" object") %
                                ex.what() % exp_itr->first).str());
    }
    ++exp_itr;
    ++act_itr;
  }
  std::cout << "return\n";
}

/**
 * check that the content body of the expected, from the test case, and
 * actual, from the response, is the same.
 */
void check_content_body_json(std::istream &expected, std::istream &actual) {
  pt::ptree exp_tree, act_tree;

  try {
    pt::read_json(expected, exp_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        (boost::format("%1%, while reading expected JSON.") % ex.what()).str());
  }

  try {
    pt::read_json(actual, act_tree);
  } catch (const std::exception &ex) {
    throw std::runtime_error(
        (boost::format("%1%, while reading actual JSON.") % ex.what()).str());
  }

  expected.seekg(0);
  std::cout << "=== expected ===\n";
  do {
    std::string line;
    std::getline(expected, line);
    std::cout << line << "\n";
  } while (expected.good());
  actual.seekg(0);
  std::cout << "=== actual ===\n";
  do {
    std::string line;
    std::getline(actual, line);
    std::cout << line << "\n";
  } while (actual.good());

  // and check the results for equality
  check_recursive_tree_json(exp_tree, act_tree);
}

/**
 * Check the response from cgimap against the expected test result
 * from the test file.
 */
void check_response(std::istream &expected, std::istream &actual) {
  typedef std::map<std::string, std::string> dict;

  // check that, for some headers that we get, they are the same
  // as we expect.
  const dict expected_headers = read_headers(expected, "---");
  const dict actual_headers = read_headers(actual, "");

  BOOST_FOREACH(const dict::value_type &val, expected_headers) {
    if ((val.first.size() > 0) && (val.first[0] == '!')) {
      dict::const_iterator itr = actual_headers.find(val.first.substr(1));
      if (itr != actual_headers.end()) {
        throw std::runtime_error(
            (boost::format(
                 "Expected not to find header `%1%', but it is present.") %
             itr->first).str());
      }
    } else {
      dict::const_iterator itr = actual_headers.find(val.first);
      if (itr == actual_headers.end()) {
        throw std::runtime_error(
            (boost::format("Expected header `%1%: %2%', but didn't find it in "
                           "actual response.") %
             val.first % val.second).str());
      }
      if (!val.second.empty()) {
        if (val.second != itr->second) {
          throw std::runtime_error(
              (boost::format(
                   "Header key `%1%'; expected `%2%' but got `%3%'.") %
               val.first % val.second % itr->second).str());
        }
      }
    }
  }

  // now check the body, if there is one. we judge this by whether we expect a
  // Content-Type header.
  if (expected_headers.count("Content-Type") > 0) {
    const std::string content_type =
        expected_headers.find("Content-Type")->second;
    if (content_type.substr(0, 8) == "text/xml") {
      check_content_body_xml(expected, actual);

    } else if (content_type.substr(0, 9) == "text/json") {
      check_content_body_json(expected, actual);

    } else {
      throw std::runtime_error(
          (boost::format("Cannot yet handle tests with Content-Type: %1%.") %
           content_type).str());
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
void run_test(fs::path test_case, rate_limiter &limiter,
              const std::string &generator, routes &route,
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
    throw std::runtime_error(
        (boost::format("%1%, in %2% test.") % ex.what() % test_case).str());
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
      std::cerr << "Test directory " << test_directory
                << " should be a directory, but isn't.";
      return 99;
    }
    if (fs::is_regular_file(data_file) == false) {
      std::cerr << "Test directory should contain data file at " << data_file
                << ", but does not.";
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
    vm.insert(std::make_pair(std::string("file"),
                             po::variable_value(data_file.native(), false)));

    boost::shared_ptr<backend> data_backend = make_staticxml_backend();
    boost::shared_ptr<data_selection::factory> factory =
        data_backend->create(vm);
    rate_limiter limiter(vm);
    routes route;

    BOOST_FOREACH(fs::path test_case, test_cases) {
      std::string generator =
          (boost::format(PACKAGE_STRING " (test %1%)") % test_case).str();
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
