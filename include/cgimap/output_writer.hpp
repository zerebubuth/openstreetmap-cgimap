#ifndef OUTPUT_WRITER_HPP
#define OUTPUT_WRITER_HPP

#include <string>
#include <stdexcept>
#include <boost/noncopyable.hpp>

/**
 * base class of all writers.
 */
class output_writer : public boost::noncopyable {
public:
  virtual ~output_writer() throw();

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
