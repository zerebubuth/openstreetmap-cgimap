#ifndef OUTPUT_FORMATTER_HPP
#define OUTPUT_FORMATTER_HPP

#include "bbox.hpp"
#include <pqxx/pqxx>
#include <stdexcept>

/**
 * What type of element the formatter is starting to write.
 */
enum element_type {
  element_type_node,
  element_type_way,
  element_type_relation
};

/**
 * Base type for different output formats. Hopefully this is general
 * enough to encompass most formats that we want to produce. Assuming,
 * of course, that we want any other formats ;-)
 */
struct output_formatter {
  virtual ~output_formatter();

  // called once to start the document - this will be the first call
  // to this object after construction.
  virtual void start_document() = 0;

  // called once to end the document - there will be no calls after this
  // one. this will be called, even if an error has occurred.
  virtual void end_document() = 0;

  // this is called if there is an error during reading data from the
  // database. hopefully this is a very very rare occurrance.
  virtual void error(const std::exception &e) = 0;

  // write a bounds object to the document. this seems to be generally used
  // to record the box used by a map call.
  virtual void write_bounds(const bbox &bounds) = 0;

  // start a type of element. this is called once for nodes, ways or 
  // relations. between the start and end called for a particular element
  // type only write_* functions for that type will be called.
  virtual void start_element_type(element_type type, size_t num_elements) = 0; 

  // end a type of element. this is called once for nodes, ways or relations 
  virtual void end_element_type(element_type type) = 0; 

  // output a single node given that node's row and an iterator over its tags
  virtual void write_node(const pqxx::result::tuple &t, pqxx::result &tags) = 0;

  // output a single way given a row and iterators for nodes and tags
  virtual void write_way(const pqxx::result::tuple &t, pqxx::result &nodes, pqxx::result &tags) = 0;

  // output a single relation given a row and iterators over members and tags
  virtual void write_relation(const pqxx::result::tuple &t, pqxx::result &members, pqxx::result &tags) = 0;
};

#endif /* OUTPUT_FORMATTER_HPP */
