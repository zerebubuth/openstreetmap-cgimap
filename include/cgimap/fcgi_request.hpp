#ifndef FCGI_REQUEST_HPP
#define FCGI_REQUEST_HPP

#include "cgimap/request.hpp"
#include <boost/scoped_ptr.hpp>

struct fcgi_request : public request {
  explicit fcgi_request(int socket);
  virtual ~fcgi_request();
  const char *get_param(const char *key);
  boost::shared_ptr<output_buffer> get_buffer();
  int accept_r();
  void finish();
  static int open_socket(const std::string &, int);
private:
  struct pimpl;
  boost::scoped_ptr<pimpl> m_impl;
  boost::shared_ptr<output_buffer> m_buffer;
};

#endif /* FCGI_REQUEST_HPP */
