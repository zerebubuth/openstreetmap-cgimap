#ifndef OUTPUT_BUFFER_HPP
#define OUTPUT_BUFFER_HPP

/**
 * Implement this interface to provide custom output.
 */
struct output_buffer {
  virtual int write(const char *buffer, int len) = 0;
  virtual int written() = 0;
  virtual int close() = 0;
  virtual void flush() = 0;
  virtual ~output_buffer();
};

#endif /* OUTPUT_BUFFER_HPP */
