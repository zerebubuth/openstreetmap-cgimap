#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <stdexcept>
#include <iostream>
#include "cgimap/xml_writer.hpp"

struct xml_writer::pimpl_ {
  xmlTextWriterPtr writer;
};

xml_writer::xml_writer(const std::string &file_name, bool indent)
    : pimpl(new pimpl_()) {
  // allocate the text writer "object"
  pimpl->writer = xmlNewTextWriterFilename(file_name.c_str(), 0);

  // check the return value
  if (pimpl->writer == NULL) {
    throw std::runtime_error("error creating xml writer.");
  }

  init(indent);
}

static int wrap_write(void *context, const char *buffer, int len) {
  output_buffer *out = static_cast<output_buffer *>(context);

  if (out == 0) {
    throw xml_writer::write_error("Output buffer was NULL in wrap_write().");
  }

  return out->write(buffer, len);
}

static int wrap_close(void *context) {
  output_buffer *out = static_cast<output_buffer *>(context);

  if (out == 0) {
    throw xml_writer::write_error("Output buffer was NULL in wrap_close().");
  }

  return out->close();
}

// create a new XML writer using writer callback functions
xml_writer::xml_writer(boost::shared_ptr<output_buffer> &out, bool indent)
    : pimpl(new pimpl_()) {
  xmlOutputBufferPtr output_buffer =
      xmlOutputBufferCreateIO(wrap_write, wrap_close, out.get(), NULL);

  // allocate a writer using the output buffer object
  pimpl->writer = xmlNewTextWriter(output_buffer);

  // check the return value
  if (pimpl->writer == NULL) {
    // free the output buffer
    free(output_buffer);

    throw std::runtime_error("error creating xml writer.");
  }

  init(indent);
}

void xml_writer::init(bool indent) {
  // maybe enable indenting
  if (indent) {
    xmlTextWriterSetIndent(pimpl->writer, 1);
  }

  // start the document
  if (xmlTextWriterStartDocument(pimpl->writer, NULL, "UTF-8", NULL) < 0) {
    throw write_error("error creating document element.");
  }
}

xml_writer::~xml_writer() throw() {
  // close and flush the xml writer object. note - if this fails then
  // there isn't much we can do, as this object is going to be deleted
  // anyway.
  try {
    xmlTextWriterEndDocument(pimpl->writer);
  } catch (...) {
    // don't do anything here or we risk FUBARing the entire program.
    // it might not be possible to end the document because the output
    // stream went away. if so, then there is nothing to do but try
    // and reclaim the extra memory.
  }
  xmlFreeTextWriter(pimpl->writer);

  // finally, delete the PIMPL object
  delete pimpl;
}

void xml_writer::start(const std::string &name) {
  if (xmlTextWriterStartElement(pimpl->writer, BAD_CAST name.c_str()) < 0) {
    throw write_error("cannot start element.");
  }
}

void xml_writer::attribute(const std::string &name, const std::string &value) {
  int rc = xmlTextWriterWriteAttribute(pimpl->writer, BAD_CAST name.c_str(),
                                       BAD_CAST value.c_str());
  if (rc < 0) {
    throw write_error("cannot write attribute.");
  }
}

void xml_writer::attribute(const std::string &name, const char *value) {
  const char *c_str = (value == NULL) ? "" : value;
  int rc = xmlTextWriterWriteAttribute(pimpl->writer, BAD_CAST name.c_str(),
                                       BAD_CAST c_str);
  if (rc < 0) {
    throw write_error("cannot write attribute.");
  }
}

void xml_writer::attribute(const std::string &name, double value) {
  int rc = xmlTextWriterWriteFormatAttribute(
      pimpl->writer, BAD_CAST name.c_str(), "%.7f", value);
  if (rc < 0) {
    throw write_error("cannot write double-precision attribute.");
  }
}

void xml_writer::attribute(const std::string &name, int32_t value) {
  int rc = xmlTextWriterWriteFormatAttribute(
      pimpl->writer, BAD_CAST name.c_str(), "%" PRId32, value);
  if (rc < 0) {
    throw write_error("cannot write int32_t attribute.");
  }
}

void xml_writer::attribute(const std::string &name, int64_t value) {
  int rc = xmlTextWriterWriteFormatAttribute(
      pimpl->writer, BAD_CAST name.c_str(), "%" PRId64, value);
  if (rc < 0) {
    throw write_error("cannot write int64_t attribute.");
  }
}

void xml_writer::attribute(const std::string &name, uint32_t value) {
  int rc = xmlTextWriterWriteFormatAttribute(
      pimpl->writer, BAD_CAST name.c_str(), "%" PRIu32, value);
  if (rc < 0) {
    throw write_error("cannot write uint32_t attribute.");
  }
}

void xml_writer::attribute(const std::string &name, uint64_t value) {
  int rc = xmlTextWriterWriteFormatAttribute(
      pimpl->writer, BAD_CAST name.c_str(), "%" PRIu64, value);
  if (rc < 0) {
    throw write_error("cannot write uint64_t attribute.");
  }
}

void xml_writer::attribute(const std::string &name, bool value) {
  const char *str = value ? "true" : "false";
  int rc = xmlTextWriterWriteAttribute(pimpl->writer, BAD_CAST name.c_str(),
                                       BAD_CAST str);
  if (rc < 0) {
    throw write_error("cannot write boolean attribute.");
  }
}

void xml_writer::text(const std::string &t) {
  if (xmlTextWriterWriteString(pimpl->writer, BAD_CAST t.c_str()) < 0) {
    throw write_error("cannot write text string.");
  }
}

void xml_writer::end() {
  if (xmlTextWriterEndElement(pimpl->writer) < 0) {
    throw write_error("cannot end element.");
  }
}

void xml_writer::flush() {
  if (xmlTextWriterFlush(pimpl->writer) < 0) {
    throw write_error("cannot flush output stream");
  }
}

void xml_writer::error(const std::string &s) {
  start("error");
  text(s);
  end();
}

// TODO: move this to its own file
output_buffer::~output_buffer() {}

xml_writer::write_error::write_error(const char *message)
    : std::runtime_error(message) {}
