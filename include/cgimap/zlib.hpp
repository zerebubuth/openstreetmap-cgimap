#ifndef ZLIB_HPP
#define ZLIB_HPP

#include "cgimap/config.hpp"

#ifndef HAVE_LIBZ
#error This file should not be included when zlib is not available.
#endif

#include <boost/shared_ptr.hpp>

#include <zlib.h>
#include "cgimap/output_buffer.hpp"

/**
 * Compresses an output stream.
 */
class zlib_output_buffer : public output_buffer {
public:
  /**
   * Output mode.
   */
  enum mode { zlib, gzip };

  /**
   * Methods.
   */
  zlib_output_buffer(boost::shared_ptr<output_buffer> o, mode m);
  zlib_output_buffer(const zlib_output_buffer &old);
  virtual ~zlib_output_buffer(void);
  virtual int write(const char *buffer, int len);
  virtual int written(void);
  virtual int close(void);
  virtual void flush();

private:
  void flush_output(void);

  boost::shared_ptr<output_buffer> out;
  // keep track of bytes written because the z_stream struct doesn't seem to
  // update unless its flushed.
  size_t bytes_in;
  z_stream stream;
  char outbuf[4096];
};

#endif /* ZLIB_HPP */
