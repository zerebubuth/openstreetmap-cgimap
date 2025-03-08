/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef ZLIB_HPP
#define ZLIB_HPP

#ifndef HAVE_LIBZ
#error This file should not be included when zlib is not available.
#endif

const unsigned int ZLIB_COMPLETE_CHUNK = 16384;

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
  zlib_output_buffer(output_buffer& o, mode m);
  zlib_output_buffer(const zlib_output_buffer &old);
  ~zlib_output_buffer() override = default;
  int write(const char *buffer, int len) override;
  int written() const override;
  int close() override;
  void flush() override;

private:
  void flush_output();

  output_buffer& out;
  // keep track of bytes written because the z_stream struct doesn't seem to
  // update unless its flushed.
  size_t bytes_in = 0;
  z_stream stream{};
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
  std::string decompress(const std::string& input);
  ~ZLibBaseDecompressor();

protected:
  ZLibBaseDecompressor() = default;
  explicit ZLibBaseDecompressor(int windowBits);


private:
  char inbuf[ZLIB_COMPLETE_CHUNK];
  char outbuf[ZLIB_COMPLETE_CHUNK];
  z_stream stream{};
  bool use_decompression{false};
};

class ZLibDecompressor : public ZLibBaseDecompressor {
public:
  ZLibDecompressor();
};

class GZipDecompressor : public ZLibBaseDecompressor {
public:
  GZipDecompressor();
};

class IdentityDecompressor : public ZLibBaseDecompressor {
public:
  IdentityDecompressor();
};

#endif /* ZLIB_HPP */
