/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/request.hpp"
#include "cgimap/output_buffer.hpp"

#include <stdexcept>

#include <fmt/core.h>

namespace {
// set the default HTTP response headers
void set_default_headers(request &req) {
  const char *origin = req.get_param("HTTP_ORIGIN");
  if (origin) {
    req.add_header("Access-Control-Allow-Credentials", "true")
       .add_header("Access-Control-Allow-Methods", http::list_methods(req.methods()))
       .add_header("Access-Control-Allow-Origin", std::string(origin))
       .add_header("Access-Control-Max-Age", "1728000");
  }
}
} // anonymous namespace


request& request::status(int code) {
  check_workflow(status_HEADERS);
  m_status = code;
  return *this;
}

request& request::add_header(const std::string &key, const std::string &value) {
  check_workflow(status_HEADERS);
  m_headers.emplace_back(key, value);
  return *this;
}

request& request::add_success_header(const std::string &key, const std::string &value) {
  check_workflow(status_HEADERS);
  m_success_headers.emplace_back(key, value);
  return *this;
}

output_buffer& request::get_buffer() {
  check_workflow(status_BODY);
  return get_buffer_internal();
}

int request::put(const char *ptr, int len) {
  return get_buffer().write(ptr, len);
}

int request::put(std::string_view str) {
  return get_buffer().write(str);
}

void request::flush() { get_buffer().flush(); }

void request::finish() {
  check_workflow(status_FINISHED);
  finish_internal();
}

void request::check_workflow(workflow_status this_stage) {
  if (m_workflow_status < this_stage) {
    if ((status_HEADERS > m_workflow_status) &&
        (status_HEADERS <= this_stage)) {
      // must be in HEADERS workflow stage to set headers.
      m_workflow_status = status_HEADERS;
      set_default_headers(*this);
    }

    if ((status_BODY > m_workflow_status) && (status_BODY <= this_stage)) {
      // must be in BODY workflow stage to write output
      m_workflow_status = status_BODY;

      // some HTTP headers are only returned in case the request was successful
      auto headers = m_headers;
      if (m_status == 200)
        headers.insert(headers.end(), m_success_headers.begin(), m_success_headers.end());

      write_header_info(m_status, headers);
    }

    m_workflow_status = this_stage;

  } else if (m_workflow_status > this_stage) {
    // oops - workflow is more advanced than the function which called
    // this, so a workflow violation has occurred.
    throw std::runtime_error(fmt::format("Can't move backwards in the request workflow from {:d} to {:d}.",
        static_cast<int>(m_workflow_status), static_cast<int>(this_stage)));
  }
}

void request::reset() {
  m_workflow_status = status_NONE;
  m_status = 500;
  m_headers.clear();
  m_success_headers.clear();
  m_methods = http::method::GET | http::method::HEAD | http::method::OPTIONS;
}

void request::set_default_methods(http::method m) {
  m_methods = m;
}

http::method request::methods() const {
  return m_methods;
}
