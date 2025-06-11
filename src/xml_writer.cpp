/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2025 by the openstreetmap-cgimap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/xml_writer.hpp"

#include <array>
#include <stdexcept>

#include <fmt/core.h>
#include <fmt/compile.h>

// C-style callback functions are called from xmlOutputBufferCreateIO.
// Functions cannot throw, return -1 if an error occurred.
static int wrap_write(void *context, const char *buffer, int len) noexcept {
  auto *out = static_cast<output_buffer *>(context);

  if (out == nullptr) {
    return -1;
  }

  return out->write(buffer, len);
}

static int wrap_close(void *context) noexcept {
  auto *out = static_cast<output_buffer *>(context);

  if (out == nullptr) {
    return -1;
  }

  return out->close();
}

static void callback_generic_error(void* ctx, const char* msg, ...)
{
  // we can't do much with the error here, in particular we don't want to write it to stderr.
  // xml_writer methods still get an rc=-1 from libxml2 functions and throw an exception.
  // so nothing to do here...
}

// create a new XML writer using writer callback functions
xml_writer::xml_writer(output_buffer &out, bool indent) {
  xmlOutputBufferPtr output_buffer =
      xmlOutputBufferCreateIO(wrap_write, wrap_close, &out, nullptr);

  // set our own error handler (xmlpp::parser.cpp also calls this function, and we may be
  // left with a non-functioning xmlpp error handler if we don't define our own here)
  xmlSetGenericErrorFunc(output_buffer->context, &callback_generic_error);

  // allocate a writer using the output buffer object
  writer = xmlNewTextWriter(output_buffer);

  // check the return value
  if (writer == nullptr) {
    // free the output buffer
    free(output_buffer);

    throw std::runtime_error("error creating xml writer.");
  }

  // maybe enable indenting
  if (indent) {
    xmlTextWriterSetIndent(writer, 1);
  }

  // start the document
  if (xmlTextWriterStartDocument(writer, nullptr, "UTF-8", nullptr) < 0) {
    throw write_error("error creating document element.");
  }
}


xml_writer::~xml_writer() noexcept {
  // close and flush the xml writer object. note - if this fails then
  // there isn't much we can do, as this object is going to be deleted
  // anyway.

  xmlTextWriterEndDocument(writer);
  xmlFreeTextWriter(writer);
}

void xml_writer::start(const char *name) {
  if (xmlTextWriterStartElement(writer, reinterpret_cast<const xmlChar *>(name)) < 0) {
    throw write_error("cannot start element.");
  }
}

void xml_writer::attribute(const char *name, const std::string &value) {
  int rc = writeAttribute(name, value.c_str());
  if (rc < 0) {
    throw write_error("cannot write attribute.");
  }
}

void xml_writer::attribute(const char *name, const char *value) {
  const char *c_str = value ? value : "";
  int rc = writeAttribute(name, c_str);
  if (rc < 0) {
    throw write_error("cannot write attribute.");
  }
}

void xml_writer::attribute(const char *name, double value) {
  const char* str = nullptr;

#if FMT_VERSION >= 90000
  std::array<char, 384> buf;
  constexpr size_t max_chars = buf.size() - 1;
  auto [end, n_written] = fmt::format_to_n(buf.begin(), max_chars, FMT_COMPILE("{:.7f}"), value);
  if (n_written > max_chars)
    throw write_error("cannot convert double-precision attribute to string.");
  *end = '\0'; // Null terminate string
  str = buf.data();
#else
  auto s = fmt::format("{:.7f}", value);
  str = s.c_str();
#endif

  int rc = writeAttribute(name, str);
  if (rc < 0) {
    throw write_error("cannot write double-precision attribute.");
  }
}

void xml_writer::attribute(const char *name, bool value) {
  const char *str = value ? "true" : "false";
  int rc = writeAttribute(name, str);
  if (rc < 0) {
    throw write_error("cannot write boolean attribute.");
  }
}

void xml_writer::text(const char* t) {
  if (xmlTextWriterWriteString(writer, reinterpret_cast<const xmlChar *>(t)) < 0) {
    throw write_error("cannot write text string.");
  }
}

void xml_writer::end() {
  if (xmlTextWriterEndElement(writer) < 0) {
    throw write_error("cannot end element.");
  }
}

void xml_writer::flush() {
  if (xmlTextWriterFlush(writer) < 0) {
    throw write_error("cannot flush output stream");
  }
}

void xml_writer::error(const std::string &s) {
  start("error");
  text(s);
  end();
}

int xml_writer::writeAttribute(const char* name, const char* value) {
  int rc = xmlTextWriterWriteAttribute(
    writer,
    reinterpret_cast<const xmlChar *>(name),
    reinterpret_cast<const xmlChar *>(value));
  return rc;
}

