/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef OUTPUT_WRITER_HPP
#define OUTPUT_WRITER_HPP

#include <string>
#include <stdexcept>


/**
 * base class of all writers.
 */
class output_writer {
public:

  output_writer(const output_writer &) = delete;
  output_writer& operator=(const output_writer &) = delete;
  output_writer(output_writer &&) = default;
  output_writer& operator=(output_writer &&) = default;

  output_writer() = default;

  virtual ~output_writer() noexcept = default;

  /* write an error to the output. normally, we'd detect errors *before*
   * starting to write. so this is a very rare case, for example when the
   * database disappears during the request processing.
   */
  virtual void error(const std::string &) = 0;

  // flushes the output buffer
  virtual void flush() = 0;

  /**
   * Thrown when writing fails.
   */
  class write_error : public std::runtime_error {
  public:
    explicit write_error(const char *message);
  };
};

#endif /* OUTPUT_WRITER_HPP */
