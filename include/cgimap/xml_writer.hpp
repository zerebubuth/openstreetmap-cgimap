/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#ifndef WRITER_HPP
#define WRITER_HPP

#include "cgimap/output_buffer.hpp"
#include "cgimap/output_writer.hpp"

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include <string>
#include <charconv>
#include <array>

/**
 * Writes UTF-8 output to a file or stdout.
 */
class xml_writer : public output_writer {
public:
  xml_writer(const xml_writer &) = delete;
  xml_writer& operator=(const xml_writer &) = delete;
  xml_writer(xml_writer &&) = default;
  xml_writer& operator=(xml_writer &&) = default;

  // create a new XML writer using writer callback functions
  explicit xml_writer(output_buffer &out, bool indent = false);

  // closes and flushes the XML writer
  ~xml_writer() noexcept override;

  // begin a new element with the given name
  void start(const char *name);
  inline void start(const std::string &name) { start(name.c_str()); }

  // write an attribute of the form name="value" to the current element
  void attribute(const char *name, const std::string &value);

  // write a mysql string, which can be null
  void attribute(const char *name, const char *value);

  // overloaded versions of writeAttribute for convenience
  void attribute(const char *name, double value);
  void attribute(const char *name, bool value);

  template<typename T>
  inline void attribute(const std::string &name, T value) { attribute(name.c_str(), value); }

  template<typename TInteger, std::enable_if_t<std::is_integral_v<TInteger>, bool> = true>
  void attribute(const char *name, TInteger value) {
    static_assert(sizeof(value) <= 8);
    std::array<char, 32> buf;

    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size() - 1, value);
    if (ec != std::errc())
      throw write_error("cannot convert integer attribute to string.");

    *ptr = '\0'; // Null terminate string
    int rc = writeAttribute(name, buf.data());
    if (rc < 0) {
      throw write_error("cannot write integer attribute.");
    }
  }

  // write a child text element
  void text(const char* t);
  inline void text(const std::string &t) { text(t.c_str()); }

  // end the current element
  void end();

  // flushes the output buffer
  void flush() override;

  void error(const std::string &) override;

private:
  int writeAttribute(const char* name, const char* value);

  xmlTextWriterPtr writer;
};

#endif /* WRITER_HPP */
