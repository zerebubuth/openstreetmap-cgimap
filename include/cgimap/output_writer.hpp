#ifndef OUTPUT_WRITER_HPP
#define OUTPUT_WRITER_HPP

#include <memory>
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

  virtual ~output_writer() noexcept;

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
    write_error(const char *message);
  };
};

#endif /* OUTPUT_WRITER_HPP */
