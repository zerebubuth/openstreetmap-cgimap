#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <boost/shared_ptr.hpp>
#include "output_buffer.hpp"

struct request {
  virtual ~request();
  virtual const char *get_param(const char *key) = 0;
  virtual int put(const char *, int) = 0;
  virtual int put(const std::string &) = 0;
  virtual boost::shared_ptr<output_buffer> get_buffer() = 0;
  virtual std::string cors_headers() = 0;
  virtual void flush() = 0;
  virtual int accept_r() = 0;
  virtual void finish() = 0;
};

#endif /* REQUEST_HPP */
