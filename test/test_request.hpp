#ifndef TEST_TEST_REQUEST_HPP
#define TEST_TEST_REQUEST_HPP

#include <chrono>
#include <fstream>
#include <map>
#include <sstream>

#include "cgimap/request.hpp"
#include "cgimap/output_buffer.hpp"

/**
 * Mock output buffer so that we can get back an in-memory result as a string
 * backed buffer.
 */
struct test_output_buffer : public output_buffer {
  explicit test_output_buffer(std::ostream &out);
  virtual ~test_output_buffer();
  virtual int write(const char *buffer, int len);
  virtual int written();
  virtual int close();
  virtual void flush();

private:
  std::ostream &m_out;
  int m_written;
};

/**
 * Mock request so that we can control the headers and get back the response
 * body for comparison to what we expect.
 */
struct test_request : public request {
  test_request();

  /// implementation of request interface
  virtual ~test_request();
  virtual const char *get_param(const char *key);
  virtual const std::string get_payload();
  void set_payload(const std::string&);

  virtual void dispose();

  /// getters and setters for the input headers and output response
  void set_header(const std::string &k, const std::string &v);
  std::stringstream &buffer();

  std::chrono::system_clock::time_point get_current_time() const;
  void set_current_time(const std::chrono::system_clock::time_point &now);

  int response_status() const;

protected:
  virtual void write_header_info(int status, const headers_t &headers);
  virtual std::shared_ptr<output_buffer> get_buffer_internal();
  virtual void finish_internal();

private:
  int m_status;
  std::stringstream m_output;
  std::map<std::string, std::string> m_params;
  std::chrono::system_clock::time_point m_now;
  std::string m_payload;
};

#endif /* TEST_TEST_REQUEST_HPP */
