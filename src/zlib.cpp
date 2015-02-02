#include <assert.h>
#include <algorithm>

#include "cgimap/zlib.hpp"
#include "cgimap/output_writer.hpp"

zlib_output_buffer::zlib_output_buffer(boost::shared_ptr<output_buffer> o,
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

zlib_output_buffer::~zlib_output_buffer(void) {}

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

int zlib_output_buffer::close(void) {
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

int zlib_output_buffer::written(void) { return bytes_in; }

void zlib_output_buffer::flush_output(void) {
  out->write(outbuf, sizeof(outbuf) - stream.avail_out);

  stream.next_out = (Bytef *)outbuf;
  stream.avail_out = sizeof(outbuf);
}

void zlib_output_buffer::flush() { flush_output(); }
