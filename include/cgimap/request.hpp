#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <boost/shared_ptr.hpp>
#include "cgimap/output_buffer.hpp"

struct request {
  virtual ~request();
  virtual const char *get_param(const char *key) = 0;
  virtual boost::shared_ptr<output_buffer> get_buffer() = 0;
  virtual void finish() = 0;

  int put(const char *, int);
  int put(const std::string &);
  void flush();
};

#endif /* REQUEST_HPP */
