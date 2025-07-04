/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#include <cassert>
#include <algorithm>
#include <cstring>

#include "cgimap/zlib.hpp"
#include "cgimap/logger.hpp"
#include "cgimap/output_writer.hpp"

zlib_output_buffer::zlib_output_buffer(output_buffer& o,
                                       zlib_output_buffer::mode m)
    : out(o), bytes_in(0) {
  int windowBits;

  switch (m) {
  case mode::zlib:
    windowBits = 15;
    break;
  case mode::gzip:
    windowBits = 15 + 16;
    break;
  }

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;

  if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    throw output_writer::write_error("deflateInit2 failed");
  }

  stream.next_in = nullptr;
  stream.avail_in = 0;
  stream.next_out = (Bytef *)outbuf;
  stream.avail_out = sizeof(outbuf);
}

int zlib_output_buffer::write(const char *buffer, int len) noexcept {
  assert(stream.avail_in == 0);

  if (len > 0) {
    int status = 0;

    stream.next_in = (Bytef *)buffer;
    stream.avail_in = len;

    for (status = deflate(&stream, Z_NO_FLUSH);
         status == Z_OK && stream.avail_in > 0;
         status = deflate(&stream, Z_NO_FLUSH)) {
      flush_output();
    }

    if (status != Z_OK) {
      logger::message("deflate failed");
      return -1;
    }

    if (stream.avail_out == 0) {
      flush_output();
    }
  }

  bytes_in += len;

  return len;
}

int zlib_output_buffer::close() noexcept {
  int status = 0;

  assert(stream.avail_in == 0);

  for (status = deflate(&stream, Z_FINISH); status == Z_OK;
       status = deflate(&stream, Z_FINISH)) {
    flush_output();
  }

  if (status == Z_STREAM_END) {
    out.write(outbuf, sizeof(outbuf) - stream.avail_out);
  } else {
    logger::message("deflate failed");
    return -1;
  }

  if (deflateEnd(&stream) != Z_OK) {
    logger::message("deflateEnd failed");
    return -1;
  }

  return out.close();
}

int zlib_output_buffer::written() const { return bytes_in; }

int zlib_output_buffer::flush_output() noexcept {
  int rc = out.write(outbuf, sizeof(outbuf) - stream.avail_out);

  stream.next_out = (Bytef *)outbuf;
  stream.avail_out = sizeof(outbuf);

  return (rc < 0 ? -1 : 0);
}

int zlib_output_buffer::flush() noexcept { return flush_output(); }

/*******************************************************************************/

// parts adopted from https://github.com/rudi-cilibrasi/zlibcomplete

ZLibBaseDecompressor::ZLibBaseDecompressor(int windowBits) {

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  stream.avail_in = 0;
  stream.next_in = Z_NULL;
  if (inflateInit2(&stream, windowBits) != Z_OK) {
    throw std::bad_alloc();
  }
  use_decompression = true;
}

ZLibBaseDecompressor::~ZLibBaseDecompressor() {
  if (use_decompression)
    inflateEnd(&stream);
}

std::string ZLibBaseDecompressor::decompress(const std::string& input) {

  std::string result;

  if (!use_decompression)
    return input;

  for (std::size_t offset = 0; offset < input.length(); offset += ZLIB_COMPLETE_CHUNK) {

    unsigned int bytes_left = input.length() - offset;
    unsigned int bytes_wanted = std::min(ZLIB_COMPLETE_CHUNK, bytes_left);

    std::memcpy(inbuf, input.data() + offset, bytes_wanted);

    stream.avail_in = bytes_wanted;
    stream.next_in = (Bytef *) inbuf;

    if (stream.avail_in == 0) {
      break;
    }

    do {
      stream.avail_out = ZLIB_COMPLETE_CHUNK;
      stream.next_out = (Bytef *) outbuf;
      int ret = inflate(&stream, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      switch (ret) {
      case Z_NEED_DICT:
          ret = Z_DATA_ERROR;
          [[fallthrough]];
          /* no break */
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
          inflateEnd(&stream);
          throw std::runtime_error("Zlib decompression failed");
      }

      unsigned int have = ZLIB_COMPLETE_CHUNK - stream.avail_out;
      result += std::string(outbuf, have);
    } while (stream.avail_out == 0);
  }
  return result;
}

GZipDecompressor::GZipDecompressor() : ZLibBaseDecompressor(15+16) { }

ZLibDecompressor::ZLibDecompressor() : ZLibBaseDecompressor(15) { }

IdentityDecompressor::IdentityDecompressor() : ZLibBaseDecompressor() { }




