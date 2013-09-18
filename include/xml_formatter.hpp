#ifndef XML_FORMATTER_HPP
#define XML_FORMATTER_HPP

#include "output_formatter.hpp"
#include "xml_writer.hpp"
#include <boost/scoped_ptr.hpp>

/**
 * Outputs an XML-formatted document, i.e: the OSM document type we all know
 * and love.
 */
class xml_formatter 
  : public output_formatter {
private:
  boost::shared_ptr<xml_writer> writer;
  
  void write_tags(const tags_t &tags);
  void write_common(const element_info &elem);

public:
  // NOTE: takes ownership of the writer!
  xml_formatter(xml_writer *w);
  virtual ~xml_formatter();

  mime::type mime_type() const;

  void start_document(const std::string &generator);
  void end_document();
  void write_bounds(const bbox &bounds);
  void start_element_type(element_type type); 
  void end_element_type(element_type type); 
  void error(const std::exception &e);

   void write_node(const element_info &elem, double lon, double lat, const tags_t &tags);
   void write_way(const element_info &elem, const nodes_t &nodes, const tags_t &tags);
   void write_relation(const element_info &elem, const members_t &members, const tags_t &tags);

   void flush();
   void error(const std::string &);
};

#endif /* XML_FORMATTER_HPP */
