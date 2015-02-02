#ifndef OUTPUT_FORMATTER_HPP
#define OUTPUT_FORMATTER_HPP

#include "cgimap/bbox.hpp"
#include "cgimap/types.hpp"
#include "cgimap/mime_types.hpp"
#include <list>
#include <stdexcept>
#include <boost/optional.hpp>

/**
 * What type of element the formatter is starting to write.
 */
enum element_type {
  element_type_node,
  element_type_way,
  element_type_relation
};

struct element_info {
  // Standard meanings
  osm_id_t id, version, changeset;
  std::string timestamp;
  // Anonymous objects will not have uids or display names
  boost::optional<osm_id_t> uid;
  boost::optional<std::string> display_name;
  // If an object has been deleted
  bool visible;
};

struct member_info {
  element_type type;
  osm_id_t ref;
  std::string role;
};

typedef std::list<osm_id_t> nodes_t;
typedef std::list<member_info> members_t;
typedef std::list<std::pair<std::string, std::string> > tags_t;

/**
 * Base type for different output formats. Hopefully this is general
 * enough to encompass most formats that we want to produce. Assuming,
 * of course, that we want any other formats ;-)
 */
struct output_formatter {
  virtual ~output_formatter();

  // returns the mime type of the content that this formatter will
  // produce.
  virtual mime::type mime_type() const = 0;

  // called once to start the document - this will be the first call
  // to this object after construction. the string passed will be
  // used as the "generator" header attribute.
  virtual void start_document(const std::string &generator) = 0;

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
  virtual void start_element_type(element_type type) = 0;

  // end a type of element. this is called once for nodes, ways or relations
  virtual void end_element_type(element_type type) = 0;

  // output a single node given that node's row and an iterator over its tags
  virtual void write_node(const element_info &elem, double lon, double lat,
                          const tags_t &tags) = 0;

  // output a single way given a row and iterators for nodes and tags
  virtual void write_way(const element_info &elem, const nodes_t &nodes,
                         const tags_t &tags) = 0;

  // output a single relation given a row and iterators over members and tags
  virtual void write_relation(const element_info &elem,
                              const members_t &members, const tags_t &tags) = 0;

  // flush the current state
  virtual void flush() = 0;

  // write an error to the output stream
  virtual void error(const std::string &) = 0;
};

#endif /* OUTPUT_FORMATTER_HPP */
