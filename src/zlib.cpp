#include <cassert>
#include <algorithm>
#include <cstring>


#include "cgimap/zlib.hpp"
#include "cgimap/output_writer.hpp"

zlib_output_buffer::zlib_output_buffer(std::shared_ptr<output_buffer> o,
                                       zlib_output_buffer::mode m)
    : out(o), bytes_in(0) {
  int windowBits;

  switch (m) {
  case zlib:
    windowBits = 15;
    break;
  case gzip:
    windowBits = 15 + 16;
    break;
  default:
    throw;
  }

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;

  if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    throw output_writer::write_error("deflateInit2 failed");
  }

  stream.next_in = NULL;
  stream.avail_in = 0;
  stream.next_out = (Bytef *)outbuf;
  stream.avail_out = sizeof(outbuf);
}

zlib_output_buffer::zlib_output_buffer(const zlib_output_buffer &old)
    : out(old.out), stream(old.stream) {
  std::copy(old.outbuf, (const char *)old.stream.next_out, outbuf);
  stream.next_out = (Bytef *)outbuf + (sizeof(outbuf) - stream.avail_out);
}

zlib_output_buffer::~zlib_output_buffer(void) = default;

int zlib_output_buffer::write(const char *buffer, int len) {
  assert(stream.avail_in == 0);

  if (len > 0) {
    int status;

    stream.next_in = (Bytef *)buffer;
    stream.avail_in = len;

    for (status = deflate(&stream, Z_NO_FLUSH);
         status == Z_OK && stream.avail_in > 0;
         status = deflate(&stream, Z_NO_FLUSH)) {
      flush_output();
    }

    if (status != Z_OK) {
      throw output_writer::write_error("deflate failed");
    }

    if (stream.avail_out == 0) {
      flush_output();
    }
  }

  bytes_in += len;

  return len;
}

int zlib_output_buffer::close() {
  int status;

  assert(stream.avail_in == 0);

  for (status = deflate(&stream, Z_FINISH); status == Z_OK;
       status = deflate(&stream, Z_FINISH)) {
    flush_output();
  }

  if (status == Z_STREAM_END) {
    out->write(outbuf, sizeof(outbuf) - stream.avail_out);
  } else {
    throw output_writer::write_error("deflate failed");
  }

  if (deflateEnd(&stream) != Z_OK) {
    throw output_writer::write_error("deflateEnd failed");
  }

  return out->close();
}

int zlib_output_buffer::written() { return bytes_in; }

void zlib_output_buffer::flush_output() {
  out->write(outbuf, sizeof(outbuf) - stream.avail_out);

  stream.next_out = (Bytef *)outbuf;
  stream.avail_out = sizeof(outbuf);
}

void zlib_output_buffer::flush() { flush_output(); }

/*******************************************************************************/

// parts adopted from https://github.com/rudi-cilibrasi/zlibcomplete

ZLibBaseDecompressor::ZLibBaseDecompressor(int windowBits) {
  int retval;
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  stream.avail_in = 0;
  stream.next_in = Z_NULL;
  retval = inflateInit2(&stream, windowBits);
  if (retval != Z_OK) {
    throw std::bad_alloc();
  }
  use_decompression = true;
}

ZLibBaseDecompressor::ZLibBaseDecompressor() : use_decompression(false) { }

ZLibBaseDecompressor::~ZLibBaseDecompressor() {
  if (use_decompression)
    inflateEnd(&stream);
}

std::string ZLibBaseDecompressor::decompress(const std::string& input) {

  int ret;
  std::string result;

  if (!use_decompression)
    return input;

  for (auto offset = 0; offset < input.length(); offset += ZLIB_COMPLETE_CHUNK) {

    unsigned int bytes_left = input.length() - offset;
    unsigned int bytes_wanted = std::min(ZLIB_COMPLETE_CHUNK, bytes_left);

    std::memcpy(inbuf, input.data() + offset, bytes_wanted);

    stream.avail_in = bytes_wanted;
    stream.next_in = (Bytef *) inbuf;

    if (stream.avail_in == 0) {
      break;
    }

    do {
      unsigned int have;
      stream.avail_out = ZLIB_COMPLETE_CHUNK;
      stream.next_out = (Bytef *) outbuf;
      ret = inflate(&stream, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      switch (ret) {
      case Z_NEED_DICT:
          ret = Z_DATA_ERROR;
          /* no break */
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
          inflateEnd(&stream);
          throw std::runtime_error("Zlib decompression failed");
      }

      have = ZLIB_COMPLETE_CHUNK - stream.avail_out;
      result += std::string(outbuf, have);
    } while (stream.avail_out == 0);
  }
  return result;
}

GZipDecompressor::GZipDecompressor() : ZLibBaseDecompressor(15+16) { }

GZipDecompressor::~GZipDecompressor() = default;

ZLibDecompressor::ZLibDecompressor() : ZLibBaseDecompressor(15) { }

ZLibDecompressor::~ZLibDecompressor() = default;

IdentityDecompressor::IdentityDecompressor() : ZLibBaseDecompressor() { }

IdentityDecompressor::~IdentityDecompressor() = default;



