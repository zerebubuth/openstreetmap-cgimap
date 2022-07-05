#include "test_request.hpp"
#include "cgimap/request_helpers.hpp"

#include <cassert>

test_output_buffer::test_output_buffer(std::ostream &out, std::ostream &body)
  : m_out(out), m_body(body), m_written(0) {
}

test_output_buffer::~test_output_buffer() = default;

int test_output_buffer::write(const char *buffer, int len) {
  m_body.write(buffer, len);
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

test_request::test_request() : m_status(-1), m_payload{}  {}

test_request::~test_request() = default;

const char *test_request::get_param(const char *key) const {
  std::string key_str(key);
  auto itr = m_params.find(key_str);
  if (itr != m_params.end()) {
    return itr->second.c_str();
  } else {
    return NULL;
  }
}

const std::string test_request::get_payload() {
  return m_payload;
}

void test_request::set_payload(const std::string& payload) {
  m_payload = payload;
}

void test_request::dispose() {}

void test_request::set_header(const std::string &k, const std::string &v) {
  m_params.insert(std::make_pair(k, v));
}

std::stringstream &test_request::buffer() {
  return m_output;
}

std::stringstream &test_request::body() {
  return m_body;
}

std::stringstream &test_request::header() {
  return m_header;
}

std::chrono::system_clock::time_point test_request::get_current_time() const {
  return m_now;
}

void test_request::set_current_time(const std::chrono::system_clock::time_point &now) {
  m_now = now;
}

void test_request::write_header_info(int status, const headers_t &headers) {
  assert(m_output.tellp() == 0);
  m_status = status;

  std::stringstream hdr;
  hdr << "Status: " << status << " " << status_message(status) << "\r\n";
  for (const request::headers_t::value_type &header : headers) {
      hdr << header.first << ": " << header.second << "\r\n";
  }
  hdr << "\r\n";

  m_output << hdr.str();
  m_header << hdr.str();
}

int test_request::response_status() const {
  return m_status;
}

output_buffer& test_request::get_buffer_internal() {
  test_ob_buffer.reset(new test_output_buffer(m_output, m_body));
  return *test_ob_buffer;
}

void test_request::finish_internal() {}
