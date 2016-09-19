#include "cgimap/fcgi_request.hpp"
#include "cgimap/output_buffer.hpp"
#include "cgimap/request_helpers.hpp"
#include <boost/foreach.hpp>
#include <stdexcept>
#include <sstream>
#include <fcgiapp.h>
#include <errno.h>
#include <string.h>

using std::runtime_error;

namespace {
struct fcgi_buffer : public output_buffer {

  explicit fcgi_buffer(FCGX_Request req) : m_req(req), m_written(0) {}

  virtual ~fcgi_buffer() {}

  int write(const char *buffer, int len) {
    int bytes = FCGX_PutStr(buffer, len, m_req.out);
    if (bytes >= 0) {
      m_written += bytes;
    }
    return bytes;
  }

  int written() { return m_written; }

  int close() { return FCGX_FClose(m_req.out); }

  void flush() { FCGX_FFlush(m_req.out); }

private:
  FCGX_Request m_req;
  int m_written;
};
}

struct fcgi_request::pimpl {
  FCGX_Request req;
  boost::posix_time::ptime now;
};

fcgi_request::fcgi_request(int socket, const boost::posix_time::ptime &now) : m_impl(new pimpl) {
  // initialise FCGI
  if (FCGX_Init() != 0) {
    throw runtime_error("Couldn't initialise FCGX library.");
  }
  if (FCGX_InitRequest(&m_impl->req, socket, FCGI_FAIL_ACCEPT_ON_INTR) != 0) {
    throw runtime_error("Couldn't initialise FCGX request structure.");
  }
  m_impl->now = now;
  m_buffer = boost::shared_ptr<output_buffer>(new fcgi_buffer(m_impl->req));
}

fcgi_request::~fcgi_request() { FCGX_Free(&m_impl->req, true); }

const char *fcgi_request::get_param(const char *key) {
  return FCGX_GetParam(key, m_impl->req.envp);
}

boost::posix_time::ptime fcgi_request::get_current_time() const {
  return m_impl->now;
}

void fcgi_request::set_current_time(const boost::posix_time::ptime &now) {
  m_impl->now = now;
}

void fcgi_request::write_header_info(int status,
                                     const request::headers_t &headers) {
  std::ostringstream ostr;
  ostr << "Status: " << status << " " << status_message(status) << "\r\n";
  BOOST_FOREACH(const request::headers_t::value_type &header, headers) {
    ostr << header.first << ": " << header.second << "\r\n";
  }
  ostr << "\r\n";
  std::string data(ostr.str());
  m_buffer->write(&data[0], data.size());
}

boost::shared_ptr<output_buffer> fcgi_request::get_buffer_internal() {
  return m_buffer;
}

void fcgi_request::finish_internal() {}

void fcgi_request::dispose() { FCGX_Finish_r(&m_impl->req); }

int fcgi_request::accept_r() {
  int status = FCGX_Accept_r(&m_impl->req);
  if (status < 0) {
    if (errno != EINTR) {
      char err_buf[1024];
      std::ostringstream out;

      if (errno == ENOTSOCK) {
        out << "FCGI port or UNIX socket not set properly, please use the "
            << "--socket option (caused by ENOTSOCK).";

      } else {
        out << "error accepting request: ";
        if (strerror_r(errno, err_buf, sizeof err_buf) == 0) {
          out << err_buf;
        } else {
          out << "error encountered while getting error message";
        }
      }

      throw runtime_error(out.str());
    }
  }

  // reset status, as we re-use requests.
  reset();

  // swap out the output buffer for a new one referencing the new
  // request.
  boost::shared_ptr<output_buffer> new_buffer(new fcgi_buffer(m_impl->req));
  m_buffer.swap(new_buffer);

  return status;
}

int fcgi_request::open_socket(const std::string &path, int backlog) {
  return FCGX_OpenSocket(path.c_str(), backlog);
}
