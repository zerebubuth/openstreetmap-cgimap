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

  output_buffer() = default;

  output_buffer(const output_buffer&) = delete;
  output_buffer& operator=(const output_buffer&) = delete;

  output_buffer(output_buffer&&) = delete;
  output_buffer& operator=(output_buffer&&) = delete;
};

class identity_output_buffer : public output_buffer
{
public:
    identity_output_buffer(output_buffer& o) : out(o) {}

    int write(const char *buffer, int len) override { return out.write(buffer, len); }
    int written() override { return out.written(); }
    int close() override { return out.close(); }
    void flush() override { out.flush(); }

private:
    output_buffer& out;
};

#endif /* OUTPUT_BUFFER_HPP */
