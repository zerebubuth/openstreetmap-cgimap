/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OUTPUT_BUFFER_HPP
#define OUTPUT_BUFFER_HPP

#include <string_view>

/**
 * Implement this interface to provide custom output.
 */
struct output_buffer {
  virtual int write(const char *buffer, int len) = 0;
  virtual int write(std::string_view str) { return write(str.data(), str.size()); }
  virtual int written() const = 0;
  virtual int close() = 0;
  virtual void flush() = 0;
  virtual ~output_buffer() = default;

  output_buffer() = default;

  output_buffer(const output_buffer&) = delete;
  output_buffer& operator=(const output_buffer&) = delete;

  output_buffer(output_buffer&&) = delete;
  output_buffer& operator=(output_buffer&&) = delete;
};

class identity_output_buffer : public output_buffer
{
public:
    explicit identity_output_buffer(output_buffer& o) : out(o) {}

    int write(const char *buffer, int len) override { return out.write(buffer, len); }
    int written() const override { return out.written(); }
    int close() override { return out.close(); }
    void flush() override { out.flush(); }

    identity_output_buffer(const identity_output_buffer&) = delete;
    identity_output_buffer& operator=(const identity_output_buffer&) = delete;

    identity_output_buffer(identity_output_buffer&&) = delete;
    identity_output_buffer& operator=(identity_output_buffer&&) = delete;

private:
    output_buffer& out;
};

#endif /* OUTPUT_BUFFER_HPP */
