#ifndef XML_FORMATTER_HPP
#define XML_FORMATTER_HPP

#include "output_formatter.hpp"
#include "cache.hpp"
#include "changeset.hpp"
#include "writer.hpp"

/**
 * Outputs an XML-formatted document, i.e: the OSM document type we all know
 * and love.
 */
class xml_formatter 
  : public output_formatter {
private:
  xml_writer &writer;
  cache<long int, changeset> &changeset_cache;
  
  void write_tags(pqxx::result &tags);

public:
  xml_formatter(xml_writer &w, cache<long int, changeset> &cc);
  virtual ~xml_formatter();

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

#endif /* XML_FORMATTER_HPP */
