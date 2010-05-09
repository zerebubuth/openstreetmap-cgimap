#ifndef JSON_WRITER_HPP
#define JSON_WRITER_HPP

#include <string>
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include "output_buffer.hpp"

/**
 * nice(ish) interface to writing a JSON file.
 */
class json_writer 
  : public boost::noncopyable {
public:
  // create a json writer using a callback object for output
  json_writer(boost::shared_ptr<output_buffer> &out, bool indent = false);

  // closes and flushes the buffer
  ~json_writer();

  void start_object();
  void object_key(const std::string &s);
  void end_object();

  void start_array();
  void end_array();

  void entry_bool(bool b);
  void entry_int(int i);
  void entry_int(long int i);
  void entry_int(long long int i);
  void entry_double(double d);
  void entry_string(const std::string &s);

private:
  // PIMPL idiom
  struct pimpl_;
  pimpl_ *pimpl;
};

#endif /* JSON_WRITER_HPP */
