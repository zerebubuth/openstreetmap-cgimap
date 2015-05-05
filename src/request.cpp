#include "cgimap/request.hpp"
#include "cgimap/output_buffer.hpp"

#include <stdexcept>

#include <boost/format.hpp>

namespace {
// set the default HTTP response headers
void set_default_headers(request &req) {
  const char *origin = req.get_param("HTTP_ORIGIN");
  if (origin) {
    req.add_header("Access-Control-Allow-Credentials", "true");
    req.add_header("Access-Control-Allow-Methods", "GET");
    req.add_header("Access-Control-Allow-Origin", std::string(origin));
    req.add_header("Access-Control-Max-Age", "1728000");
  }
}
} // anonymous namespace

request::request()
    : m_workflow_status(status_NONE), m_status(500), m_headers() {}

request::~request() {}

void request::status(int code) {
  check_workflow(status_HEADERS);
  m_status = code;
}

void request::add_header(const std::string &key, const std::string &value) {
  check_workflow(status_HEADERS);
  m_headers.push_back(std::make_pair(key, value));
}

boost::shared_ptr<output_buffer> request::get_buffer() {
  check_workflow(status_BODY);
  return get_buffer_internal();
}

int request::put(const char *ptr, int len) {
  return get_buffer()->write(ptr, len);
}

int request::put(const std::string &str) {
  return get_buffer()->write(str.c_str(), str.size());
}

void request::flush() { get_buffer()->flush(); }

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
      write_header_info(m_status, m_headers);
    }

    m_workflow_status = this_stage;

  } else if (m_workflow_status > this_stage) {
    // oops - workflow is more advanced than the function which called
    // this, so a workflow violation has occurred.
    throw std::runtime_error((boost::format("Can't move backwards in the "
                                            "request workflow from %1% to "
                                            "%2%.") %
                              m_workflow_status % this_stage).str());
  }
}

void request::reset() {
  m_workflow_status = status_NONE;
  m_status = 500;
  m_headers.clear();
}
