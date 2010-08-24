#ifndef JSON_FORMATTER_HPP
#define JSON_FORMATTER_HPP

#include "output_formatter.hpp"
#include "cache.hpp"
#include "changeset.hpp"
#include "json_writer.hpp"

/**
 * Outputs a JSON-formatted document, which might be useful for javascript
 * or other applications that don't want to parse XML.
 */
class json_formatter 
  : public output_formatter {
private:
  json_writer &writer;
  cache<long int, changeset> &changeset_cache;

  void write_tags(pqxx::result &tags);

public:
  json_formatter(json_writer &w, cache<long int, changeset> &cc);
  virtual ~json_formatter();

  void start_document();
  void end_document();
  void write_bounds(const bbox &bounds);
  void start_element_type(element_type type, size_t num_elements); 
  void end_element_type(element_type type); 
  void error(const std::exception &e);
  void write_node(const pqxx::result::tuple &t, pqxx::result &tags);
  void write_way(const pqxx::result::tuple &t, pqxx::result &nodes, pqxx::result &tags);
  void write_relation(const pqxx::result::tuple &t, pqxx::result &members, pqxx::result &tags);
};

#endif /* JSON_FORMATTER_HPP */
