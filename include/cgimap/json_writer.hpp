#ifndef JSON_WRITER_HPP
#define JSON_WRITER_HPP

#include <memory>
#include <string>
#include <stdexcept>
#include "cgimap/output_buffer.hpp"
#include "cgimap/output_writer.hpp"

/**
 * nice(ish) interface to writing a JSON file.
 */
class json_writer : public output_writer {
public:
  json_writer(const json_writer &) = delete;
  json_writer& operator=(const json_writer &) = delete;
  json_writer(json_writer &&) = default;
  json_writer& operator=(json_writer &&) = default;

  // create a json writer using a callback object for output
  json_writer(std::shared_ptr<output_buffer> &out, bool indent = false);

  // closes and flushes the buffer
  ~json_writer() noexcept;

  void start_object();
  void object_key(const std::string &s);
  void end_object();

  void start_array();
  void end_array();

  void entry_bool(bool b);
  void entry_int(int32_t i);
  void entry_int(int64_t i);
  void entry_int(uint32_t i);
  void entry_int(uint64_t i);
  void entry_double(double d);
  void entry_string(const std::string &s);

  void flush();

  void error(const std::string &);

private:
  // PIMPL idiom
  struct pimpl_;
  std::unique_ptr<pimpl_> pimpl;
  std::shared_ptr<output_buffer> out;
};

#endif /* JSON_WRITER_HPP */
