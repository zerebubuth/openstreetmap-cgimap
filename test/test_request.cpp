#include "test_request.hpp"
#include "cgimap/request_helpers.hpp"

#include <boost/foreach.hpp>

namespace bt = boost::posix_time;

test_output_buffer::test_output_buffer(std::ostream &out)
  : m_out(out), m_written(0) {
}

test_output_buffer::~test_output_buffer() {}

int test_output_buffer::write(const char *buffer, int len) {
  m_out.write(buffer, len);
  m_written += len;
  return len;
}

int test_output_buffer::written() {
  return m_written;
}

int test_output_buffer::close() {
  return 0;
}

void test_output_buffer::flush() {}

test_request::test_request() : m_status(-1) {}

test_request::~test_request() {}

const char *test_request::get_param(const char *key) {
  std::string key_str(key);
  std::map<std::string, std::string>::iterator itr = m_params.find(key_str);
  if (itr != m_params.end()) {
    return itr->second.c_str();
  } else {
    return NULL;
  }
}

void test_request::dispose() {}

void test_request::set_header(const std::string &k, const std::string &v) {
  m_params.insert(std::make_pair(k, v));
}

std::stringstream &test_request::buffer() {
  return m_output;
}

bt::ptime test_request::get_current_time() const {
  return m_now;
}

void test_request::set_current_time(const bt::ptime &now) {
  m_now = now;
}

void test_request::write_header_info(int status, const headers_t &headers) {
  assert(m_output.tellp() == 0);
  m_status = status;
  m_output << "Status: " << status << " " << status_message(status) << "\r\n";
  BOOST_FOREACH(const request::headers_t::value_type &header, headers) {
    m_output << header.first << ": " << header.second << "\r\n";
  }
  m_output << "\r\n";
}

int test_request::response_status() const {
  return m_status;
}

boost::shared_ptr<output_buffer> test_request::get_buffer_internal() {
  return boost::shared_ptr<output_buffer>(new test_output_buffer(m_output));
}

void test_request::finish_internal() {}
