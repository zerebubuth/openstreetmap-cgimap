#ifndef ZLIB_HPP
#define ZLIB_HPP

#include <boost/shared_ptr.hpp>

#include "writer.hpp"
#include "zlib.h"

/**
 * Compresses an output stream.
 */
class zlib_output_buffer
  : public xml_writer::output_buffer {
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
  zlib_output_buffer(boost::shared_ptr<xml_writer::output_buffer> o, mode m);
  zlib_output_buffer(const zlib_output_buffer &old);
  virtual ~zlib_output_buffer(void);
  virtual int write(const char *buffer, int len);
  virtual int written(void);
  virtual int close(void);

private:
  void flush_output(void);

  boost::shared_ptr<xml_writer::output_buffer> out;
  // keep track of bytes written because the z_stream struct doesn't seem to 
  // update unless its flushed.
  size_t bytes_in;
  z_stream stream;
  char outbuf[4096];
};

#endif /* ZLIB_HPP */
