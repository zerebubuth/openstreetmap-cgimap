#ifndef JSON_FORMATTER_HPP
#define JSON_FORMATTER_HPP

#include "cgimap/output_formatter.hpp"
#include "cgimap/json_writer.hpp"

#include <chrono>

/**
 * Outputs a JSON-formatted document, which might be useful for javascript
 * or other applications that don't want to parse XML.
 */
class json_formatter : public output_formatter {
private:
  std::unique_ptr<json_writer> writer;
  bool is_in_elements_array;

  void write_tags(const tags_t &tags);
  void write_id(const element_info &elem);
  void write_common(const element_info &elem);

public:
  // NOTE: takes ownership of the writer!
  json_formatter(json_writer *w);
  virtual ~json_formatter();

  mime::type mime_type() const;

  void start_document(const std::string &generator, const std::string &root_name);
  void end_document();
  void write_bounds(const bbox &bounds);
  void start_element_type(element_type type);
  void end_element_type(element_type type);
  void start_action(action_type type);
  void end_action(action_type type);
  void error(const std::exception &e);

  void write_node(const element_info &elem, double lon, double lat,
                  const tags_t &tags);
  void write_way(const element_info &elem, const nodes_t &nodes,
                 const tags_t &tags);
  void write_relation(const element_info &elem, const members_t &members,
                      const tags_t &tags);

  void write_changeset(const changeset_info &elem,
                       const tags_t &tags,
                       bool include_comments,
                       const comments_t &comments,
                       const std::chrono::system_clock::time_point &now);

  void write_diffresult_create_modify(const element_type elem,
				      const osm_nwr_signed_id_t old_id,
				      const osm_nwr_id_t new_id,
				      const osm_version_t new_version);
  void write_diffresult_delete(const element_type elem,
			       const osm_nwr_signed_id_t old_id);

  void flush();
  void error(const std::string &);
};

#endif /* JSON_FORMATTER_HPP */
