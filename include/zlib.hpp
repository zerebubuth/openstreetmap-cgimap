#ifndef ZLIB_HPP
#define ZLIB_HPP

#include <boost/shared_ptr.hpp>

#include "writer.hpp"
#include "zlib.h"
#include "output_buffer.hpp"

/**
 * Compresses an output stream.
 */
class zlib_output_buffer
  : public output_buffer {
public:
  /**
   * Output mode.
   */
  enum mode {
     zlib,
     gzip
  };

  /**
   * Methods.
   */
  zlib_output_buffer(boost::shared_ptr<output_buffer> o, mode m);
  zlib_output_buffer(const zlib_output_buffer &old);
  virtual ~zlib_output_buffer(void);
  virtual int write(const char *buffer, int len);
  virtual int close(void);

private:
  void flush_output(void);

  boost::shared_ptr<output_buffer> out;
  z_stream stream;
  char outbuf[4096];
};

#endif /* ZLIB_HPP */
