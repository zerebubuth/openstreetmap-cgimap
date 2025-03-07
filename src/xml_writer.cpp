/**
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This file is part of openstreetmap-cgimap (https://github.com/zerebubuth/openstreetmap-cgimap/).
 *
 * Copyright (C) 2009-2024 by the CGImap developer community.
 * For a full list of authors see the git log.
 */

#include "cgimap/xml_writer.hpp"

#include <array>
#include <stdexcept>

#include <fmt/core.h>
#include <fmt/compile.h>


static int wrap_write(void *context, const char *buffer, int len) {
  auto *out = static_cast<output_buffer *>(context);

  if (out == nullptr) {
    throw xml_writer::write_error("Output buffer was NULL in wrap_write().");
  }

  return out->write(buffer, len);
}

static int wrap_close(void *context) {
  auto *out = static_cast<output_buffer *>(context);

  if (out == nullptr) {
    throw xml_writer::write_error("Output buffer was NULL in wrap_close().");
  }

  return out->close();
}

// create a new XML writer using writer callback functions
xml_writer::xml_writer(output_buffer &out, bool indent) {
  xmlOutputBufferPtr output_buffer =
      xmlOutputBufferCreateIO(wrap_write, wrap_close, &out, nullptr);

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
  try {
    xmlTextWriterEndDocument(writer);
  } catch (...) {
    // don't do anything here or we risk FUBARing the entire program.
    // it might not be possible to end the document because the output
    // stream went away. if so, then there is nothing to do but try
    // and reclaim the extra memory.
  }
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

// TODO: move this to its own file

xml_writer::write_error::write_error(const char *message)
    : std::runtime_error(message) {}
