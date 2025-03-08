/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/fcgi_request.hpp"
#include "cgimap/http.hpp"
#include "cgimap/options.hpp"
#include "cgimap/output_buffer.hpp"

#include <fmt/core.h>

#include <array>
#include <stdexcept>
#include <sstream>
#include <fcgiapp.h>
#include <cerrno>
#include <cstring>


namespace {
struct fcgi_buffer : public output_buffer {

  fcgi_buffer() = delete;
  fcgi_buffer(const fcgi_buffer&) = delete;
  fcgi_buffer& operator=(const fcgi_buffer&) = delete;
  fcgi_buffer(fcgi_buffer&&) = delete;
  fcgi_buffer& operator=(fcgi_buffer&&) = delete;

  explicit fcgi_buffer(FCGX_Request req) : m_req(req) {}

  ~fcgi_buffer() override = default;

  int write(const char *buffer, int len) override {
    int bytes = FCGX_PutStr(buffer, len, m_req.out);
    if (bytes >= 0) {
      m_written += bytes;
    }
    return bytes;
  }

  [[nodiscard]] int written() const override { return m_written; }

  int close() override { return FCGX_FClose(m_req.out); }

  void flush() override { FCGX_FFlush(m_req.out); }

private:
  FCGX_Request m_req;
  int m_written{0};
};
}

struct fcgi_request::pimpl {
  FCGX_Request req;
  std::chrono::system_clock::time_point now;
};

fcgi_request::fcgi_request(int socket, const std::chrono::system_clock::time_point &now) : m_impl(std::make_unique<pimpl>()) {
  // initialise FCGI
  if (FCGX_Init() != 0) {
    throw std::runtime_error("Couldn't initialise FCGX library.");
  }
  if (FCGX_InitRequest(&m_impl->req, socket, FCGI_FAIL_ACCEPT_ON_INTR) != 0) {
    throw std::runtime_error("Couldn't initialise FCGX request structure.");
  }
  m_impl->now = now;
  m_buffer = std::make_unique<fcgi_buffer>(m_impl->req);
}

fcgi_request::~fcgi_request() { FCGX_Free(&m_impl->req, true); }

const char *fcgi_request::get_param(const char *key) const {
  return FCGX_GetParam(key, m_impl->req.envp);
}

std::string fcgi_request::get_payload() {

  // fetch and parse the content length
  const char *content_length_str = FCGX_GetParam("CONTENT_LENGTH", m_impl->req.envp);
  const char *content_encoding = FCGX_GetParam("HTTP_CONTENT_ENCODING",  m_impl->req.envp);

  auto content_encoding_handler = http::get_content_encoding_handler(
         std::string(content_encoding == nullptr ? "" : content_encoding));

  unsigned long content_length = 0;
  unsigned long curr_content_length = 0;
  unsigned long result_length = 0;

  if (content_length_str)
    content_length = http::parse_content_length(content_length_str);

  std::string result{};

  while ((curr_content_length = FCGX_GetStr(content_buffer.data(), BUFFER_LEN, m_impl->req.in)) > 0)
  {
      std::string content(content_buffer.data(), curr_content_length);
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
  }

  if (content_length > 0 && result_length != content_length)
    throw http::server_error("HTTP Header field 'Content-Length' differs from actual payload length");

  return result;
}

std::chrono::system_clock::time_point fcgi_request::get_current_time() const {
  return m_impl->now;
}

void fcgi_request::set_current_time(const std::chrono::system_clock::time_point &now) {
  m_impl->now = now;
}

void fcgi_request::write_header_info(int status, const http::headers_t &headers) {
  m_buffer->write(http::format_header(status, headers));
}

output_buffer& fcgi_request::get_buffer_internal() {
  return *m_buffer;
}

void fcgi_request::finish_internal() {}

void fcgi_request::dispose() { FCGX_Finish_r(&m_impl->req); }

int fcgi_request::accept_r() {
  int status = FCGX_Accept_r(&m_impl->req);
  if (status < 0) {
    if (errno != EINTR) {
      std::ostringstream out;

      if (errno == ENOTSOCK) {
        out << "FCGI port or UNIX socket not set properly, please use the "
               "--socket option (caused by ENOTSOCK).";

      } else {
        char err_buf[1024];
        out << "error accepting request: ";
        if (strerror_r(errno, err_buf, sizeof err_buf) == 0) {   // NOLINT(modernize-use-nullptr)
          out << err_buf;
        } else {
          out << "error encountered while getting error message";
        }
      }

      throw std::runtime_error(out.str());
    }
  }

  // reset status, as we re-use requests.
  reset();

  // swap out the output buffer for a new one referencing the new
  // request.
  m_buffer = std::make_unique<fcgi_buffer>(m_impl->req);

  return status;
}

int fcgi_request::open_socket(const std::string &path, int backlog) {
  return FCGX_OpenSocket(path.c_str(), backlog);
}
