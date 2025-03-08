/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef TEXT_WRITER_HPP
#define TEXT_WRITER_HPP

#include <string>

#include "cgimap/output_buffer.hpp"
#include "cgimap/output_writer.hpp"

/**
 * Writes UTF-8 output to a file or stdout.
 */
class text_writer : public output_writer {
public:
  text_writer(const text_writer &) = delete;
  text_writer& operator=(const text_writer &) = delete;
  text_writer(text_writer &&) = default;

  // create a new text writer using writer callback functions
  explicit text_writer(output_buffer &out, bool indent = false);

  // closes and flushes the text writer
  ~text_writer() noexcept override;

  // begin a new element with the given name
  void start(const std::string &name);

  // write a child text element
  void text(const std::string &t);

  // end the current element
  void end();

  // flushes the output buffer
  void flush() override;

  void error(const std::string &) override;

private:
  output_buffer& out;

};

#endif /* TEXT_WRITER_HPP */
