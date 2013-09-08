#ifndef FCGI_REQUEST_HPP
#define FCGI_REQUEST_HPP

#include "request.hpp"
#include <boost/scoped_ptr.hpp>

struct fcgi_request : public request {
  explicit fcgi_request(int socket);
  virtual ~fcgi_request();
  const char *get_param(const char *key);
  int put(const char *, int);
  int put(const std::string &);
  boost::shared_ptr<output_buffer> get_buffer();
  std::string cors_headers();
  void flush();
  int accept_r();
  void finish();
  static int open_socket(const std::string &, int);
private:
  struct pimpl;
  boost::scoped_ptr<pimpl> m_impl;
};

#endif /* FCGI_REQUEST_HPP */
