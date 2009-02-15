#ifndef WRITER_HPP
#define WRITER_HPP

#include <string>
#include <mysql++.h>

/**
 * Writes UTF-8 output to a file or stdout.
 */
class xml_writer {
public:
 
  /**
   * Implement this interface to provide custom output.
   */
  struct output_buffer {
    virtual int write(const char *buffer, int len) = 0;
    virtual int close() = 0;
    virtual ~output_buffer();
  };

  /**
   * Thrown when writing fails.
   */
  class write_error
    : public std::runtime_error {
  public:
    write_error(const char *message);
  };

  // create a new XML writer writing to file_name, which can be 
  // "-" for stdout.
  xml_writer(const std::string &file_name, bool indent = false);

  // create a new XML writer using writer callback functions
  xml_writer(output_buffer &out, bool indent = false);

  // closes and flushes the XML writer
  ~xml_writer();

  // begin a new element with the given name
  void start(const std::string &name);

  // write an attribute of the form name="value" to the current element
  void attribute(const std::string &name, const std::string &value);

  // write a mysql string, which can be null
  void attribute(const std::string &name, const mysqlpp::String &value);

  // overloaded versions of writeAttribute for convenience
  void attribute(const std::string &name, double value);
  void attribute(const std::string &name, int value);
  void attribute(const std::string &name, long int value);
  void attribute(const std::string &name, long long int value);
  void attribute(const std::string &name, bool value);

  // write a child text element
  void text(const std::string &t);
  
  // end the current element
  void end();

  // flushes the output buffer
  void flush();

private:

  // shared initialisation code
  void init(bool indent);

  // PIMPL ideom
  struct pimpl_;
  pimpl_ *pimpl;

};

#endif /* WRITER_HPP */
