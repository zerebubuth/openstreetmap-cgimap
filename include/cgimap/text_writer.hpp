#ifndef TEXT_WRITER_HPP
#define TEXT_WRITER_HPP

#include <memory>
#include <string>
#include <stdexcept>
#include "cgimap/output_buffer.hpp"
#include "cgimap/output_writer.hpp"
#include <cinttypes>

/**
 * Writes UTF-8 output to a file or stdout.
 */
class text_writer : public output_writer {
public:
  text_writer(const text_writer &) = delete;
  text_writer& operator=(const text_writer &) = delete;
  text_writer(text_writer &&) = default;
  text_writer& operator=(text_writer &&) = default;

  // create a new text writer writing to file_name, which can be
  // "-" for stdout.
  text_writer(const std::string &file_name, bool indent = false);

  // create a new text writer using writer callback functions
  text_writer(std::shared_ptr<output_buffer> &out, bool indent = false);

  // closes and flushes the text writer
  ~text_writer() noexcept;

  // begin a new element with the given name
  void start(const std::string &name);

  // write a child text element
  void text(const std::string &t);

  // end the current element
  void end();

  // flushes the output buffer
  void flush();

  void error(const std::string &);

private:
  std::shared_ptr<output_buffer> out;

};

#endif /* TEXT_WRITER_HPP */
