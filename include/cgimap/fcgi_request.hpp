#ifndef FCGI_REQUEST_HPP
#define FCGI_REQUEST_HPP

#include "cgimap/request.hpp"
#include <boost/scoped_ptr.hpp>

struct fcgi_request : public request {
  explicit fcgi_request(int socket);
  virtual ~fcgi_request();
  const char *get_param(const char *key);

  int accept_r();
  static int open_socket(const std::string &, int);
  void dispose();

protected:
  void write_header_info(int status, const request::headers_t &headers);
  boost::shared_ptr<output_buffer> get_buffer_internal();
  void finish_internal();

private:
  struct pimpl;
  boost::scoped_ptr<pimpl> m_impl;
  boost::shared_ptr<output_buffer> m_buffer;
};

#endif /* FCGI_REQUEST_HPP */
