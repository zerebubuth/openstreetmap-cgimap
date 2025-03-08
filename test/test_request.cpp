/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "test_request.hpp"
#include "cgimap/options.hpp"

#include <cassert>
#include <fmt/core.h>

test_output_buffer::test_output_buffer(std::ostream &out, std::ostream &body)
  : m_out(out), m_body(body) {
}

int test_output_buffer::write(const char *buffer, int len) {
  m_body.write(buffer, len);
  m_out.write(buffer, len);
  m_written += len;
  return len;
}

int test_output_buffer::written() const {
  return m_written;
}

int test_output_buffer::close() {
  return 0;
}

void test_output_buffer::flush() {}

const char *test_request::get_param(const char *key) const {
  std::string key_str(key);
  auto itr = m_params.find(key_str);
  if (itr != m_params.end()) {
    return itr->second.c_str();
  } else {
    return nullptr;
  }
}

std::string test_request::get_payload() {

  // TODO: still a bit too much duplication from fcgi_request.cpp::get_payload

  // fetch and parse the content length
  const char *content_length_str = m_params.find("CONTENT_LENGTH") != m_params.end() ? m_params["CONTENT_LENGTH"].c_str() : nullptr;
  const char *content_encoding = m_params.find("HTTP_CONTENT_ENCODING") != m_params.end() ? m_params["HTTP_CONTENT_ENCODING"].c_str() : nullptr;

  auto content_encoding_handler = http::get_content_encoding_handler(
         std::string(content_encoding == nullptr ? "" : content_encoding));

  unsigned long content_length = 0;
  unsigned long result_length = 0;

  if (content_length_str)
    content_length = http::parse_content_length(content_length_str);

  std::string result;

  std::string content(m_payload);
  result_length += content.length();

  // Decompression according to Content-Encoding header (null op, if header is not set)
  try {
    std::string content_decompressed = content_encoding_handler->decompress(content);
    result += content_decompressed;
  } catch (std::bad_alloc& e) {
      throw http::server_error("Decompression failed due to memory issue");
  } catch (std::runtime_error& e) {
      throw http::bad_request("Payload cannot be decompressed according to Content-Encoding");
  }

  if (result.length() > global_settings::get_payload_max_size())
     throw http::payload_too_large(fmt::format("Payload exceeds limit of {:d} bytes", global_settings::get_payload_max_size()));

  if (content_length > 0 && result_length != content_length)
    throw http::server_error("HTTP Header field 'Content-Length' differs from actual payload length");

  return result;
}

void test_request::set_payload(const std::string& payload) {
  m_payload = payload;
}

void test_request::dispose() {}

void test_request::set_header(const std::string &k, const std::string &v) {
  m_params[k] = v;  // requirement: same key can be set multiple times!
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

void test_request::write_header_info(int status, const http::headers_t &headers) {
  assert(m_output.tellp() == 0);
  m_status = status;

  auto hdr = http::format_header(status, headers);
  m_output << hdr;
  m_header << hdr;
}

int test_request::response_status() const {
  return m_status;
}

output_buffer& test_request::get_buffer_internal() {
  test_ob_buffer = std::make_unique<test_output_buffer>(m_output, m_body);
  return *test_ob_buffer;
}

void test_request::finish_internal() {}
