/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEST_TEST_REQUEST_HPP
#define TEST_TEST_REQUEST_HPP

#include <chrono>
#include <map>
#include <sstream>

#include "cgimap/request.hpp"
#include "cgimap/output_buffer.hpp"

/**
 * Mock output buffer so that we can get back an in-memory result as a string
 * backed buffer.
 */
struct test_output_buffer : public output_buffer {
  explicit test_output_buffer(std::ostream &out, std::ostream &body);
  ~test_output_buffer() override = default;

  test_output_buffer(const test_output_buffer&) = delete;
  test_output_buffer& operator=(const test_output_buffer&) = delete;

  test_output_buffer(test_output_buffer&&) = delete;
  test_output_buffer& operator=(test_output_buffer&&) = delete;

  int write(const char *buffer, int len) override;
  [[nodiscard]] int written() const override;
  int close() override;
  void flush() override;

private:
  std::ostream &m_out;
  std::ostream &m_body;
  int m_written{0};
};

/**
 * Mock request so that we can control the headers and get back the response
 * body for comparison to what we expect.
 */
struct test_request : public request {
  test_request() = default;

  /// implementation of request interface
  ~test_request() override = default;
  const char *get_param(const char *key) const override;
  std::string get_payload() override;
  void set_payload(const std::string&);

  void dispose() override;

  /// getters and setters for the input headers and output response
  void set_header(const std::string &k, const std::string &v);
  std::stringstream &buffer();
  std::stringstream &body();
  std::stringstream &header();

  std::chrono::system_clock::time_point get_current_time() const override;
  void set_current_time(const std::chrono::system_clock::time_point &now);

  int response_status() const;

protected:
  void write_header_info(int status, const http::headers_t &headers) override;
  output_buffer& get_buffer_internal() override;
  void finish_internal() override;

private:
  int m_status{-1};
  std::stringstream m_output;
  std::stringstream m_header;
  std::stringstream m_body;
  std::map<std::string, std::string> m_params;
  std::chrono::system_clock::time_point m_now;
  std::string m_payload;
  std::unique_ptr<test_output_buffer> test_ob_buffer;
};

#endif /* TEST_TEST_REQUEST_HPP */
