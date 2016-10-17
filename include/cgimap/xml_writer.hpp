#ifndef WRITER_HPP
#define WRITER_HPP

#include <string>
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include "cgimap/output_buffer.hpp"
#include "cgimap/output_writer.hpp"
#include <inttypes.h>

/**
 * Writes UTF-8 output to a file or stdout.
 */
class xml_writer : public output_writer {
public:
  // create a new XML writer writing to file_name, which can be
  // "-" for stdout.
  xml_writer(const std::string &file_name, bool indent = false);

  // create a new XML writer using writer callback functions
  xml_writer(boost::shared_ptr<output_buffer> &out, bool indent = false);

  // closes and flushes the XML writer
  ~xml_writer() throw();

  // begin a new element with the given name
  void start(const std::string &name);

  // write an attribute of the form name="value" to the current element
  void attribute(const std::string &name, const std::string &value);

  // write a mysql string, which can be null
  void attribute(const std::string &name, const char *value);

  // overloaded versions of writeAttribute for convenience
  void attribute(const std::string &name, double value);
  void attribute(const std::string &name, int32_t value);
  void attribute(const std::string &name, int64_t value);
  void attribute(const std::string &name, uint32_t value);
  void attribute(const std::string &name, uint64_t value);
  void attribute(const std::string &name, bool value);

  // write a child text element
  void text(const std::string &t);

  // end the current element
  void end();

  // flushes the output buffer
  void flush();

  void error(const std::string &);

private:
  // shared initialisation code
  void init(bool indent);

  // PIMPL ideom
  struct pimpl_;
  pimpl_ *pimpl;
};

#endif /* WRITER_HPP */
