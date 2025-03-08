/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */


#ifndef BROTLI_HPP
#define BROTLI_HPP

#if HAVE_BROTLI

#include <array>
#include <cstdint>

#include <brotli/decode.h>
#include <brotli/encode.h>

#include "cgimap/output_buffer.hpp"


/**
 * Compresses an output stream.
 */
class brotli_output_buffer : public output_buffer {
public:

  brotli_output_buffer(output_buffer& o);
  brotli_output_buffer(const brotli_output_buffer &old) = delete;
  ~brotli_output_buffer() override = default;
  int write(const char *buffer, int len) override;
  int written() const override;
  int close() override;
  void flush() override;

private:
  int compress(const char *data, int data_length, bool last);

  BrotliEncoderState *state_ = nullptr;
  std::array<uint8_t, 16384> buff;

  output_buffer& out;
  // keep track of bytes written
  size_t bytes_in = 0;
  bool flushed{false};
};

#endif

#endif
