#ifndef JSON_WRITER_HPP
#define JSON_WRITER_HPP

#include <string>
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include "cgimap/output_buffer.hpp"
#include "cgimap/output_writer.hpp"

/**
 * nice(ish) interface to writing a JSON file.
 */
class json_writer : public output_writer {
public:
  // create a json writer using a callback object for output
  json_writer(boost::shared_ptr<output_buffer> &out, bool indent = false);

  // closes and flushes the buffer
  ~json_writer() throw();

  void start_object();
  void object_key(const std::string &s);
  void end_object();

  void start_array();
  void end_array();

  void entry_bool(bool b);
  void entry_int(int i);
  void entry_int(unsigned long int i);
  void entry_int(unsigned long long int i);
  void entry_double(double d);
  void entry_string(const std::string &s);

  void flush();

  void error(const std::string &);

private:
  // PIMPL idiom
  struct pimpl_;
  pimpl_ *pimpl;
};

#endif /* JSON_WRITER_HPP */
