#ifndef ZLIB_HPP
#define ZLIB_HPP

#include "cgimap/config.hpp"

#ifndef HAVE_LIBZ
#error This file should not be included when zlib is not available.
#endif

const unsigned int ZLIB_COMPLETE_CHUNK = 16384;

#include <memory>
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
  zlib_output_buffer(std::shared_ptr<output_buffer> o, mode m);
  zlib_output_buffer(const zlib_output_buffer &old);
  virtual ~zlib_output_buffer();
  virtual int write(const char *buffer, int len);
  virtual int written();
  virtual int close();
  virtual void flush();

private:
  void flush_output();

  std::shared_ptr<output_buffer> out;
  // keep track of bytes written because the z_stream struct doesn't seem to
  // update unless its flushed.
  size_t bytes_in;
  z_stream stream;
  char outbuf[4096];
};

/*******************************************************************************/

// parts adopted from https://github.com/rudi-cilibrasi/zlibcomplete

class ZLibBaseDecompressor {
public:
  /**
  * @brief Decompression function for gzip using std::string.
  *
  * Accepts a std::string of any size containing compressed data.  Returns
  * as much gzip uncompressed data as possible.  Call this function over
  * and over with all the compressed data in a stream in order to decompress
  * the entire stream.
  * @param input Any amount of data to decompress.
  * @retval std::string containing the decompressed data.
  */
  virtual std::string decompress(const std::string& input);

protected:
  ZLibBaseDecompressor();
  ZLibBaseDecompressor(int windowBits);
  ~ZLibBaseDecompressor();

private:
  char inbuf[ZLIB_COMPLETE_CHUNK];
  char outbuf[ZLIB_COMPLETE_CHUNK];
  z_stream stream;
  bool use_decompression;
};

class ZLibDecompressor : public ZLibBaseDecompressor {
public:
  ZLibDecompressor();
  virtual ~ZLibDecompressor();
};

class GZipDecompressor : public ZLibBaseDecompressor {
public:
  GZipDecompressor();
  virtual ~GZipDecompressor();
};

class IdentityDecompressor : public ZLibBaseDecompressor {
public:
  IdentityDecompressor();
  virtual ~IdentityDecompressor();
};

#endif /* ZLIB_HPP */
