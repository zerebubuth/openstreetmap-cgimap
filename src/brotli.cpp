/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */


#include "cgimap/brotli.hpp"

#include <cassert>

#if HAVE_BROTLI


brotli_output_buffer::brotli_output_buffer(output_buffer& o)
    : out(o) {

  state_ = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);

  BrotliEncoderSetParameter(state_, BROTLI_PARAM_QUALITY, 5);
}

int brotli_output_buffer::compress(const char *data, int data_length, bool last)
{
  auto operation = last ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS;
  size_t available_in = data_length;
  auto next_in = reinterpret_cast<const uint8_t *>(data);

  while (true) {
    if (last) {
      if (BrotliEncoderIsFinished(state_)) { break; }
    } else {
      if (!available_in) { break; }
    }

    auto available_out = buff.size();
    auto next_out = buff.data();

    if (!BrotliEncoderCompressStream(state_, operation, &available_in, &next_in,
                                     &available_out, &next_out, nullptr)) {
      return -1;
    }

    auto output_bytes = buff.size() - available_out;
    if (output_bytes) {
      out.write(reinterpret_cast<const char *>(buff.data()), output_bytes);
    }
  }

  bytes_in += data_length;

  return data_length;
}


int brotli_output_buffer::write(const char *buffer, int len) noexcept {
  return compress(buffer, len, false);
}

int brotli_output_buffer::close() noexcept {
  if (!flushed)
    flush();

  BrotliEncoderDestroyInstance(state_);

  return out.close();
}

int brotli_output_buffer::written() const { return bytes_in; }


int brotli_output_buffer::flush() noexcept {

  if (flushed) { // brotli does not support multiple flush operations
    return 0;
  }

  compress(nullptr, 0, true);
  flushed = true;
  return 0;
}

#endif
