/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OUTPUT_BUFFER_HPP
#define OUTPUT_BUFFER_HPP

#include <string_view>

/**
 * Implement this interface to provide custom output.
 */
struct output_buffer {
  // most methods here are noexcept, since they're also being called by C-style callbacks
  // that don't support exceptions. A return code of -1 is used instead to signal errors.
  virtual int write(const char *buffer, int len) noexcept = 0;
  virtual int write(std::string_view str) noexcept { return write(str.data(), str.size()); }
  virtual int written() const = 0;
  virtual int close() noexcept = 0;
  virtual int flush() noexcept = 0;
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
    using output_buffer::write;
    explicit identity_output_buffer(output_buffer& o) : out(o) {}

    int write(const char *buffer, int len) noexcept override { return out.write(buffer, len); }
    int written() const override { return out.written(); }
    int close() noexcept override { return out.close(); }
    int flush() noexcept override { return out.flush(); }

    ~identity_output_buffer() override = default;

    identity_output_buffer(const identity_output_buffer&) = delete;
    identity_output_buffer& operator=(const identity_output_buffer&) = delete;

    identity_output_buffer(identity_output_buffer&&) = delete;
    identity_output_buffer& operator=(identity_output_buffer&&) = delete;

private:
    output_buffer& out;
};

#endif /* OUTPUT_BUFFER_HPP */
